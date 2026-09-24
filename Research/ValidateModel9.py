"""Compare complete model-9 renders to the frozen, verified model-8 outputs."""
from pathlib import Path
import argparse,json,subprocess,sys,hashlib
from concurrent.futures import ThreadPoolExecutor
import numpy as np
ROOT=Path(__file__).resolve().parent/'NEXT_RELEASE_ORIGINAL_TEST_PACK'
sys.path.insert(0,str(ROOT))
from analyse import audio
WORK=ROOT.parents[3]/'build/mc2000-nap-2026-09-23/model9'
WORK.mkdir(exist_ok=True)
OUT=ROOT.parent/'validation_2026-09-23-nap'
old=json.loads((ROOT.parents[3]/'build/mc2000-corrected-2026-09-23/after/outputs.json').read_text())
plan={r['id']:r for r in json.loads((ROOT/'render_plan.json').read_text())}
args=argparse.ArgumentParser();args.add_argument('--prepare',action='store_true');args.add_argument('--render',action='store_true');args.add_argument('--measure',action='store_true');args=args.parse_args()
OUT=ROOT/'analysis_2026-09-24'
inv={r['file']:r for r in json.loads((OUT/'inventory.json').read_text())}
selected={key:row for key,row in plan.items() if key+'.wav' in inv and inv[key+'.wav']['sample_rate']==row['sample_rate'] and inv[key+'.wav']['duration_s']==60 and not inv[key+'.wav']['all_silent'] and not row['sidechain']}
assert all(r['neutral_reference_id'] in selected for r in selected.values())
if args.prepare:
 jobs=[];baseline_jobs=[]
 for key,row in selected.items():
  src=ROOT.parents[3]/'build/mc2000-corrected-2026-09-23'/(row['source']+'.f32')
  if not src.exists():
   rate,x=audio(ROOT/'audio'/row['source']);assert rate==row['sample_rate'];x.astype('<f4').tofile(src)
  cross=row['crossover_hz']+[1000,10000][len(row['crossover_hz'])-1:] if len(row['crossover_hz'])<3 else row['crossover_hz']
  fields=[row['sample_rate'],len(row['IN']),row['input_db'],row['output_db'],*cross]
  for i in range(4):
   if i<len(row['band_settings']):
    b=row['band_settings'][i];fields.extend([int(row['IN'][i]),int(i+1 in row['SOLO']),b['gain_db'],b['threshold_db'],b['ratio'],b['knee'],b['bite'],b['attack_ms'],b['release_ms'],{'R1':0,'R2':1,'AUTO':2}[b['mode']]])
   else:fields.extend([1,0,0,0,1,0,1,2.5,250,0])
  if row['automation']:fields.append(row['automation'])
  def job(destination):return ' '.join([json.dumps(src.as_posix()),json.dumps(destination.as_posix()),*map(str,fields)])
  jobs.append(job(WORK/(key+'.f32')))
  if key not in old:
   target=WORK/(key+'_model8.f32');old[key]=str(target);baseline_jobs.append(job(target))
 for i in range(2):(WORK/f'jobs{i}.txt').write_text('\n'.join(jobs[i::2])+'\n')
 (WORK/'baseline_jobs.txt').write_text('\n'.join(baseline_jobs)+'\n')
 (WORK/'model8_outputs.json').write_text(json.dumps(old,indent=2)+'\n')
 print('Prepared',len(jobs),'model9 and',len(baseline_jobs),'new model8 jobs',flush=True)
if args.render:
 old=json.loads((WORK/'model8_outputs.json').read_text())
 exe=ROOT.parents[3]/'build/MC2000-bite-2026-09-23/Release/MC2000OriginalPackRender.exe'
 baseline=ROOT.parents[3]/'build/MC2000-bite-2026-09-23/Release/MC2000OriginalPackRenderBaseline.exe'
 with (WORK/'baseline-render.log').open('w') as log:subprocess.run([str(baseline),str(WORK/'baseline_jobs.txt')],stdout=log,check=True)
 def run(i):
  with (WORK/f'render{i}.log').open('w') as log:subprocess.run([str(exe),str(WORK/f'jobs{i}.txt')],stdout=log,check=True)
 with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(run,range(2)))
 (OUT/'renderers.json').write_text(json.dumps({label:dict(path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest()) for label,path in [('model8',baseline),('model9',exe)]},indent=2)+'\n')
if not args.measure:sys.exit(0)
old=json.loads((WORK/'model8_outputs.json').read_text())
results=[]
for key in sorted(selected):
 path=old[key]
 row=plan[key];base=row['neutral_reference_id'];rate=row['sample_rate'];width=round(rate*.01)
 _,o=audio(ROOT/'renders'/(key+'.wav'));_,neutral=audio(ROOT/'renders'/(base+'.wav'))
 rms=lambda x:np.sqrt(np.einsum("ijk,ijk->i",x.reshape(-1,width,2),x.reshape(-1,width,2),dtype=np.float64)/(width*2))
 og=20*np.log10(np.maximum(rms(neutral),1e-15)/np.maximum(rms(o),1e-15))
 a=np.fromfile(path,dtype='<f4').reshape(-1,2);b=np.fromfile(WORK/(key+'.f32'),dtype='<f4').reshape(-1,2)
 assert a.shape==b.shape and np.isfinite(b).all()
 result=dict(id=key,diagnostic=key=='T045',diagnostic_reason='MC303 ALL: tone-wise attenuation inconsistent with independent bands under the planned preset; source of discrepancy unresolved' if key=='T045' else None,bit_identical=bool(np.array_equal(a,b)),max_audio_difference=float(np.max(abs(a.astype(float)-b))),sha256=hashlib.sha256((WORK/(key+'.f32')).read_bytes()).hexdigest())
 for label,x,directory in [('model8',a,None),('model9',b,WORK)]:
  bn=np.fromfile(old[base] if directory is None else directory/(base+'.f32'),dtype='<f4').reshape(-1,2)
  g=20*np.log10(np.maximum(rms(bn),1e-15)/np.maximum(rms(x),1e-15))
  valid=(rms(neutral)>10**(-65/20))&(rms(bn)>10**(-65/20));err=abs(g-og)
  result[label]={name:dict(mae_db=float(err[m].mean()),p95_db=float(np.quantile(err[m],.95))) if m.any() else None for name,m in [('all',valid),('active',valid&(og>.1))]}
 results.append(result)
(OUT/'model9_comparison.json').write_text(json.dumps(results,indent=2)+'\n')
for r in results:
 if r['model8']['active'] and r['model9']['active'] and not r['diagnostic']:
  print(r['id'],r['model8']['active']['mae_db'],'->',r['model9']['active']['mae_db'],flush=True)
print('Compared',len(results),flush=True)
