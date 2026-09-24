"""Band-resolved diagnostic; never fit an ALL-band render as an isolated detector."""
from pathlib import Path
import json,sys,subprocess
import numpy as np
from scipy.signal import welch
ROOT=Path(__file__).resolve().parent/'NEXT_RELEASE_ORIGINAL_TEST_PACK'
sys.path.insert(0,str(ROOT))
from analyse import audio
OUT=ROOT.parent/'validation_2026-09-23-nap'
OUT.mkdir(exist_ok=True)
if '--build' in sys.argv:
 probe=ROOT.parents[3]/'build/mc2000-nap-2026-09-23/probe'
 probe.mkdir(parents=True,exist_ok=True)
 product=ROOT.parent.parent
 (probe/'CMakeLists.txt').write_text(f'cmake_minimum_required(VERSION 3.24)\nproject(AutoNoiseProbe LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\nadd_library(AutoNoiseProbe SHARED "{product.as_posix()}/Research/AutoNoiseProbe.cpp" "{product.as_posix()}/Source/DSP/CrossoverNetwork.cpp")\ntarget_include_directories(AutoNoiseProbe PRIVATE "{product.as_posix()}/Source")\n')
 cmake='C:/cmake-3.30.1-windows-x86_64/bin/cmake.exe'
 subprocess.run([cmake,'-S',str(probe),'-B',str(probe/'out'),'-G','Visual Studio 17 2022','-A','x64'],check=True)
 subprocess.run([cmake,'--build',str(probe/'out'),'--config','Release'],check=True)
BUILD=ROOT.parents[3]/'build/mc2000-corrected-2026-09-23'
paths=json.loads((BUILD/'after/outputs.json').read_text())
_,neutral=audio(ROOT/'renders/T023.wav');_,original=audio(ROOT/'renders/T024.wav')
p0=np.fromfile(paths['T023'],dtype='<f4').reshape(-1,2).astype(float)
p1=np.fromfile(paths['T024'],dtype='<f4').reshape(-1,2).astype(float)
result=[]
for begin,end in [(8.5,11.5),(26.5,29.5),(37.5,40.5),(48.5,49.5)]:
 segment=slice(round(begin*48000),round(end*48000))
 spectra=[]
 for x in [neutral,original,p0,p1]:
  f,psd=welch(x[segment],fs=48000,nperseg=8192,noverlap=4096,axis=0)
  spectra.append(psd.mean(axis=1))
 for lo,hi in [(25,70),(200,500),(1500,6000),(13000,19000)]:
  mask=(f>=lo)&(f<=hi)
  orig=10*np.log10(spectra[0][mask].sum()/spectra[1][mask].sum())
  ours=10*np.log10(spectra[2][mask].sum()/spectra[3][mask].sum())
  b0=10*np.log10(spectra[0][mask].sum()/spectra[2][mask].sum())
  result.append(dict(seconds=[begin,end],band_hz=[lo,hi],original_attenuation_db=float(orig),ponte_attenuation_db=float(ours),neutral_difference_db=float(b0)))
(OUT/'auto_noise_bands.json').write_text(json.dumps(result,indent=2)+'\n')
for r in result:print(r)

# Candidate sweep using complete crossover reconstruction, with isolated tone
# controls held out from the noise objective. Source silence/solo startup is
# excluded from scoring; confirm the 20 us / 102 ms hypothesis against model 8.
import ctypes
lib=ctypes.CDLL(str(ROOT.parents[3]/'build/mc2000-nap-2026-09-23/probe/out/Release/AutoNoiseProbe.dll'))
fn=lib.evaluateNoise
ptr=np.ctypeslib.ndpointer(dtype=np.float32,flags='C_CONTIGUOUS')
fn.argtypes=[ptr,ptr,ctypes.c_int,ctypes.c_double,ctypes.c_double,ctypes.c_double,ctypes.c_int]
fn.restype=None
candidates=[]
plan={r['id']:r for r in json.loads((ROOT/'render_plan.json').read_text())}
for key in ['T024','T002','T010']:
 row=plan[key];base=row['neutral_reference_id']
 _,source=audio(ROOT/'audio'/row['source']);source=np.ascontiguousarray(source,dtype=np.float32)
 _,orig=audio(ROOT/'renders'/(key+'.wav'));_,neutral=audio(ROOT/'renders'/(base+'.wav'))
 baseline=np.fromfile(paths[key],dtype='<f4').reshape(-1,2)
 b0=np.fromfile(paths[base],dtype='<f4').reshape(-1,2).astype(float)
 rms=lambda x: np.sqrt(np.mean(x.astype(float).reshape(-1,480,2)**2,axis=(1,2)))
 gr=lambda x,n:20*np.log10(np.maximum(rms(n),1e-15)/np.maximum(rms(x),1e-15))
 original_gr=gr(orig,neutral);mask=(rms(neutral)>10**(-65/20))&(rms(b0)>10**(-65/20))
 mask[:100]=False
 active=mask&(original_gr>.1)
 candidate=np.empty_like(source)
 for attack,release in [(a,102) for a in [0,5,10,20,40,80,160,320,640,1000]]+[(20,r) for r in [25,50,75,150,200]]:
  fn(source,candidate,len(source),48000,attack,release,row['SOLO'][0]-1 if row['SOLO'] else -1)
  error=gr(candidate,b0)-original_gr
  item=dict(id=key,attack_us=attack,release_ms=release,mae_db=float(np.mean(abs(error[mask]))),active_mae_db=float(np.mean(abs(error[active]))))
  if key in ['T002','T010']:
   energy=lambda x:np.maximum(np.mean(x.astype(float).reshape(-1,48,2)**2,axis=(1,2)),1e-30)
   go=10*np.log10(energy(neutral)/energy(orig));gc=10*np.log10(energy(b0)/energy(candidate))
   windows=np.concatenate([np.arange(start*1000,start*1000+10) for start in [6,9,12,15,19,25,30,40]])
   item['first_10ms_rmse_db']=float(np.sqrt(np.mean((go[windows]-gc[windows])**2)))
  if attack==20 and release==102:
   item['model8_peak_difference_after_1s']=float(np.max(abs(candidate[48000:]-baseline[48000:])))
   assert item['model8_peak_difference_after_1s']<1e-6
  candidates.append(item)
(OUT/'auto_noise_candidates.json').write_text(json.dumps(candidates,indent=2)+'\n')
print('Candidates complete; minimum noise error:',min((r for r in candidates if r['id']=='T024'),key=lambda r:r['active_mae_db']),flush=True)
