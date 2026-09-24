"""Offline full-chain oversampling prototype. No production OS parameter is added.
Uses explicitly designed linear-phase FIRs and compensated offline delay.
Only steady single-tone harmonic folds are labelled alias candidates; complex
signals are compared as waveform/level changes, not mislabelled alias power.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess,time,sys
import numpy as np
from scipy.signal import firwin,resample_poly,chirp
ROOT=Path(__file__).resolve().parents[3]
BUILD=ROOT/'build/mc2000-nap-2026-09-23/oversampling'
OUT=Path(__file__).resolve().parent/'validation_2026-09-23-nap'
BUILD.mkdir(exist_ok=True,parents=True);OUT.mkdir(exist_ok=True)
EXE=ROOT/'build/MC2000-bite-2026-09-23/Release/MC2000OriginalPackRender.exe'
parser=argparse.ArgumentParser()
parser.add_argument('--measure-only',action='store_true')
parser.add_argument('--renderer',type=Path,default=EXE)
parser.add_argument('--dsp-model',type=int,required=True,help='Model compiled into the selected renderer; archived experiment used 8')
args=parser.parse_args()
EXE=args.renderer
FS=48000; t=np.arange(FS*2)/FS
signals={str(f):.7*np.sin(2*np.pi*f*t) for f in [997,7001,16001]}
signals['multitone']=sum(.16*np.sin(2*np.pi*f*t) for f in [997,7001,11003])
signals['sweep']=.7*chirp(t,100,2,21000,method='logarithmic')
signals['transient']=.7*np.sin(2*np.pi*7001*t)*((t%.2)<.035)
factors=[1,2,4,8,16]; jobs=[]; records=[]
for name,x in signals.items():
 for factor in factors:
  h=firwin(64*factor+1,1/factor,window=('kaiser',10)) if factor>1 else None
  source=resample_poly(x,factor,1,window=h) if factor>1 else x
  src=BUILD/f'{name}_{factor}_input.f32'
  if '--measure-only' not in sys.argv:np.column_stack((source,source)).astype('<f4').tofile(src)
  # ratio-1 controls at every rate; BITE sweep for compressed R1/R2/Auto.
  for mode,bite,ratio in [(0,1,1)]+[(m,b,10) for m in range(3) for b in [1,5,10]]:
   dest=BUILD/f'{name}_{factor}_{mode}_{bite}_{ratio}.f32'
   fields=[json.dumps(src.as_posix()),json.dumps(dest.as_posix()),FS*factor,4,0,0,100,785,10000]
   for band in range(4):fields += [1,0,0,-36,ratio,0,bite,.25,25,mode]
   jobs.append(' '.join(map(str,fields)))
   records.append(dict(signal=name,factor=factor,mode=mode,bite=bite,ratio=ratio,file=dest.name))
(BUILD/'jobs.txt').write_text('\n'.join(jobs)+'\n')
start=time.perf_counter()
if '--measure-only' not in sys.argv:
 with (BUILD/'render.log').open('w') as log:subprocess.run([str(EXE),str(BUILD/'jobs.txt')],stdout=log,check=True)
print('Rendered',len(records),'in',time.perf_counter()-start,flush=True)
# Do not claim renderer wall time (includes file I/O) as processing CPU.
cache={};results=[]
for r in records:
 x=np.fromfile(BUILD/r['file'],dtype='<f4').reshape(-1,2)[:,0].astype(float)
 k=r['factor']
 if k>1:x=resample_poly(x,1,k,window=firwin(64*k+1,1/k,window=('kaiser',10)))
 cache[(r['signal'],k,r['mode'],r['bite'],r['ratio'])]=x
for r in records:
 name,k,m,b,ratio=[r[key] for key in ['signal','factor','mode','bite','ratio']]
 x=cache[(name,k,m,b,ratio)];ref=cache[(name,16,m,b,ratio)]
 # Last second minus FIR edges: coherent 0.5 s segment, bins 2 Hz.
 part=slice(FS,FS+FS//2);xx=x[part];rr=ref[part]
 row={key:r[key] for key in ['signal','factor','mode','bite','ratio']}
 row['rms_dbfs']=float(20*np.log10(np.sqrt(np.mean(xx**2))))
 row['difference_to_16x_db_relative']=float(10*np.log10(max(np.mean((xx-rr)**2),1e-30)/np.mean(rr**2)))
 neutral=cache[(name,k,0,1,1)][part]
 row['effective_attenuation_db']=float(10*np.log10(np.mean(neutral**2)/np.mean(xx**2)))
 if name.isdigit():
  # Hann resolves non-integer half-second bins; sum +/- 4 bins around each
  # expected folded odd harmonic, excluding fundamental / legitimate harmonics.
  f=int(name);window=np.hanning(len(xx))
  spectrum=abs(np.fft.rfft(xx*window))**2
  fundamental_power=float(spectrum[max(1,round(f/2)-4):round(f/2)+5].sum())
  # Remove the least-squares fundamental before windowing: near-Fs/3 tones
  # have folded harmonics close to the carrier, where Hann leakage dominates.
  phase=2*np.pi*f*np.arange(len(xx))/FS
  basis=np.column_stack((np.cos(phase),np.sin(phase)))
  residual=xx-basis@np.linalg.lstsq(basis,xx,rcond=None)[0]
  spectrum=abs(np.fft.rfft(residual*window))**2
  def power(hz):
   center=round(hz/2);return float(spectrum[max(1,center-4):min(len(spectrum),center+5)].sum())
  legitimate=[n*f for n in range(1,32,2) if n*f<FS/2]
  aliases=set()
  for harmonic in range(3,32,2):
   if harmonic*f<=FS/2:continue
   folded=abs((harmonic*f+FS/2)%FS-FS/2)
   if folded>20 and all(abs(folded-hz)>20 for hz in legitimate):aliases.add(folded)
  row['fold_candidates_dbc']=float(10*np.log10(max(sum(power(hz) for hz in aliases),1e-30)/fundamental_power))
  row['fundamental_dbfs']=float(20*np.log10(2*abs(np.sum(xx*np.exp(-2j*np.pi*f*np.arange(len(xx))/FS)))/len(xx)))
 results.append(row)
(OUT/'oversampling.json').write_text(json.dumps(dict(method='FIR 64*k+1 taps, Kaiser beta=10; 2 seconds; final steady half-second; offline delay compensated; full crossover+detector+gain at internal Fs',streaming_roundtrip_fir_delay_samples=64,processing_cpu_measured=False,dsp_model=args.dsp_model,renderer_sha256=None if args.measure_only else hashlib.sha256(EXE.read_bytes()).hexdigest(),rows=results),indent=2)+'\n')
print('Measured oversampling prototype',flush=True)
