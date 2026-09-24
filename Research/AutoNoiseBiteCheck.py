from pathlib import Path
import sys,json,ctypes
import numpy as np
ROOT=Path(__file__).resolve().parent/'NEXT_RELEASE_ORIGINAL_TEST_PACK'
sys.path.insert(0,str(ROOT))
from analyse import audio
BUILD=ROOT.parents[3]/'build/mc2000-nap-2026-09-23'
lib=ctypes.CDLL(str(BUILD/'probe/out/Release/AutoNoiseProbe.dll'))
f=lib.evaluateNoiseBite
ptr=np.ctypeslib.ndpointer(dtype=np.float32,flags='C_CONTIGUOUS')
f.argtypes=[ptr,ptr,ctypes.c_int,ctypes.c_double,ctypes.c_double,ctypes.c_double,ctypes.c_int,ctypes.c_double]
f.restype=None
plan={r['id']:r for r in json.loads((ROOT/'render_plan.json').read_text())}
paths=json.loads((ROOT.parents[3]/'build/mc2000-corrected-2026-09-23/after/outputs.json').read_text())
result=[]
for key in ['T027','T028','T029','T030']:
 row=plan[key];base=row['neutral_reference_id'];length=48000*7
 _,src=audio(ROOT/'audio'/row['source']);src=np.ascontiguousarray(src[:length],dtype=np.float32)
 _,o=audio(ROOT/'renders'/(key+'.wav'));_,b=audio(ROOT/'renders'/(base+'.wav'))
 b0=np.fromfile(paths[base],dtype='<f4').reshape(-1,2)[:length]
 energy=lambda x:np.maximum(np.mean(x[:length].astype(float).reshape(-1,48,2)**2,axis=(1,2)),1e-30)
 original=10*np.log10(energy(b)/energy(o));out=np.empty_like(src)
 bite=row['band_settings'][0]['bite'];tau=3*((bite-1)/9)**1.875
 for attack in [20,320,640]:
  f(src,out,len(src),48000,attack,102,row['SOLO'][0]-1,tau)
  g=10*np.log10(energy(b0)/energy(out));error=g[6000:6050]-original[6000:6050]
  result.append(dict(id=key,attack_us=attack,bite=bite,first_50ms_rmse_db=float(np.sqrt(np.mean(error**2))),first_10ms_rmse_db=float(np.sqrt(np.mean(error[:10]**2)))))
path=ROOT.parent/'validation_2026-09-23-nap/auto_noise_bite_holdout.json'
path.write_text(json.dumps(result,indent=2)+'\n')
for r in result:print(r)
