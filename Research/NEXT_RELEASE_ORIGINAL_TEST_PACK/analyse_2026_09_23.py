"""Audit corrected original exports without overwriting historical measurements."""
from pathlib import Path
import argparse, hashlib, json, sys
import subprocess
from concurrent.futures import ThreadPoolExecutor
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
from validate import wav_info
from analyse import audio

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'analysis_2026-09-23-corrected'
BUILD=ROOT.parents[3]/'build/mc2000-corrected-2026-09-23'
PLAN=json.loads((ROOT/'render_plan.json').read_text())

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(name,data):
    OUT.mkdir(exist_ok=True)
    (OUT/name).write_text(json.dumps(data,indent=2,allow_nan=False)+'\n',encoding='utf-8')

def inventory():
    old={r['file']:r for r in json.loads((ROOT/'analysis_2026-09-20/inventory.json').read_text())}
    rows=[]
    for p in sorted((ROOT/'renders').glob('*.wav')):
        raw=p.read_bytes();h=hashlib.sha256(raw).hexdigest()
        rows.append(dict(file=p.name,sha256=h,**wav_info(raw),previous='new' if p.name not in old else
            'unchanged' if h==old[p.name]['sha256'] else 'replaced'))
    save('inventory.json',rows)
    print('Changed:',[(r['file'],r['all_silent'],r['duration_s'],r['sample_rate']) for r in rows if r['previous']!='unchanged'],flush=True)
    print('Silent exact IDs:',[r['file'] for r in rows if len(r['file'])==8 and r['all_silent']],flush=True)
    print('Missing:',[r['id'] for r in PLAN if not (ROOT/'renders'/(r['id']+'.wav')).exists()],flush=True)

def selected():
    inv={r['file']:r for r in json.loads((OUT/'inventory.json').read_text())}
    result={r['id']:r for r in PLAN if r['id']+'.wav' in inv and
        not inv[r['id']+'.wav']['all_silent'] and inv[r['id']+'.wav']['duration_s']==60 and
        inv[r['id']+'.wav']['sample_rate']==r['sample_rate'] and not r['sidechain']}
    assert all(r['neutral_reference_id'] in result for r in result.values())
    return result

def prepare(label):
    BUILD.mkdir(exist_ok=True,parents=True)
    dest=BUILD/label;dest.mkdir(exist_ok=True)
    previous=json.loads((ROOT/'analysis_2026-09-23-r1/verification.json').read_text())
    rows=selected();outputs={};jobs=[];sources=set()
    for key,r in rows.items():
        old=ROOT.parents[3]/'build/MC2000-r1-2026-09-23/renders'/(key+'.f32')
        if label=='before' and key in previous['output_sha256']:
            assert sha(old)==previous['output_sha256'][key],key
            outputs[key]=str(old);continue
        src=BUILD/(r['source']+'.f32')
        if r['source'] not in sources:
            rate,x=audio(ROOT/'audio'/r['source']);assert rate==r['sample_rate']
            src_hash=next(s['sha256'] for s in json.loads((ROOT/'manifest.json').read_text())['files'] if s['file']==r['source'])
            assert sha(ROOT/'audio'/r['source'])==src_hash
            x.astype('<f4').tofile(src);sources.add(r['source'])
        output=dest/(key+'.f32');outputs[key]=str(output)
        cross=r['crossover_hz']+[1000,10000][len(r['crossover_hz'])-1:] if len(r['crossover_hz'])<3 else r['crossover_hz']
        fields=[json.dumps(src.as_posix()),json.dumps(output.as_posix()),r['sample_rate'],len(r['IN']),r['input_db'],r['output_db'],*cross]
        for i in range(4):
            if i<len(r['band_settings']):
                b=r['band_settings'][i]
                fields.extend([int(r['IN'][i]),int(i+1 in r['SOLO']),b['gain_db'],b['threshold_db'],b['ratio'],b['knee'],b['bite'],b['attack_ms'],b['release_ms'],{'R1':0,'R2':1,'AUTO':2}[b['mode']]])
            else:fields.extend([1,0,0,0,1,0,1,2.5,250,0])
        if r['automation']:fields.append(r['automation'])
        jobs.append(' '.join(map(str,fields)))
    for i in range(3):(dest/f'jobs_{i}.txt').write_text('\n'.join(jobs[i::3])+'\n')
    (dest/'outputs.json').write_text(json.dumps(outputs,indent=2)+'\n')
    print(label,len(rows),'cases;',len(jobs),'new jobs',flush=True)

def render(label):
    exe=BUILD/'baseline-model7.exe' if label=='before' else ROOT.parents[3]/'build/MC2000-bite-2026-09-23/Release/MC2000OriginalPackRender.exe'
    def run(i):
        subprocess.run([str(exe),str(BUILD/label/f'jobs_{i}.txt')],check=True,stdout=subprocess.DEVNULL)
    with ThreadPoolExecutor(max_workers=3) as pool:list(pool.map(run,range(3)))
    save(label+'_renderer.json',dict(path=str(exe),sha256=sha(exe)))
    print(label,'render complete',flush=True)

def original():
    result=[]
    for i,j in [(2,35),(35,36),(2,37),(55,56),(56,57),(42,43),(21,23)]:
        ra,a=audio(ROOT/'renders'/f'T{i:03}.wav');rb,b=audio(ROOT/'renders'/f'T{j:03}.wav')
        assert ra==rb and a.shape==b.shape
        d=a-b;e=float(np.sum(d*d));s=float(np.sum(a*a))
        item=dict(ids=[i,j],identical=bool(np.array_equal(a,b)),peak_error=float(np.max(abs(d))),relative_error_db=10*np.log10(e/s) if e and s else None)
        result.append(item);print(item,flush=True)
    save('original_nulls.json',result)

def measure():
    maps={label:json.loads((BUILD/label/'outputs.json').read_text()) for label in ['before','after']}
    rows=selected();metrics=[];unchanged=[];hashes={};traces={}
    for key,r in rows.items():
        rate,o=audio(ROOT/'renders'/(key+'.wav'));_,n=audio(ROOT/'renders'/(r['neutral_reference_id']+'.wav'))
        width=round(rate*.01)
        rms=lambda x:np.sqrt(np.mean(x.reshape(-1,width,2)**2,axis=(1,2)))
        ln=rms(n);go=20*np.log10(np.maximum(ln,1e-15)/np.maximum(rms(o),1e-15))
        record=dict(id=key,profile=r['profile'],automation=r['automation'],metrics={})
        data={}
        for label,paths in maps.items():
            x=np.fromfile(paths[key],dtype='<f4').reshape(-1,2).astype(float)
            b=np.fromfile(paths[r['neutral_reference_id']],dtype='<f4').reshape(-1,2).astype(float)
            assert x.shape==o.shape and np.isfinite(x).all()
            levels=rms(b);g=20*np.log10(np.maximum(levels,1e-15)/np.maximum(rms(x),1e-15))
            valid=(ln>10**(-65/20))&(levels>10**(-65/20));err=abs(g-go)
            record['metrics'][label]={name:dict(count=int(mask.sum()),mae_db=float(np.mean(err[mask])),p95_db=float(np.quantile(err[mask],.95))) if mask.any() else None
                for name,mask in [('all',valid),('active',valid&(go>.1))]}
            data[label]=x
            if label=='after':hashes[key]=sha(Path(paths[key]))
            if r['automation'] or key in ['T024','T027','T028','T029','T030']:traces[key+'_'+label]=g
        changed=any(b['mode']=='AUTO' and b['bite']>1 for b in r['band_settings'])
        if not changed:
            assert np.array_equal(data['before'],data['after']),key+' changed without Auto BITE'
            unchanged.append(key)
        if r['automation'] or key in ['T024','T027','T028','T029','T030']:traces[key+'_original']=go
        metrics.append(record)
    save('comparison.json',metrics);save('after_output_hashes.json',hashes)
    save('unchanged.json',unchanged)
    np.savez_compressed(BUILD/'comparison_traces.npz',**traces)
    print(len(metrics),'compared;',len(unchanged),'bit-identical',flush=True)

def details():
    maps={label:json.loads((BUILD/label/'outputs.json').read_text()) for label in ['before','after']}
    bites=[];figdata={}
    energy=lambda x:np.mean(x[:,0].reshape(-1,48)**2,axis=1)
    for base,key in [('T002','T027'),('T002','T028'),('T010','T029'),('T010','T030')]:
        _,b=audio(ROOT/'renders'/(base+'.wav'));_,o=audio(ROOT/'renders'/(key+'.wav'))
        curves={'original':10*np.log10(np.maximum(energy(o),1e-30)/np.maximum(energy(b),1e-30))}
        for label,paths in maps.items():
            x=np.fromfile(paths[key],dtype='<f4').reshape(-1,2).astype(float)
            n=np.fromfile(paths[base],dtype='<f4').reshape(-1,2).astype(float)
            curves[label]=10*np.log10(np.maximum(energy(x),1e-30)/np.maximum(energy(n),1e-30))
        for onset in [3,6,9,12,15,19,25,30,40]:
            part=slice(onset*1000,onset*1000+50)
            item=dict(id=key,onset_s=onset,original_peak_db=float(np.max(curves['original'][part])))
            for label in ['before','after']:
                error=curves[label][part]-curves['original'][part]
                item[label]=dict(peak_db=float(np.max(curves[label][part])),rmse_db=float(np.sqrt(np.mean(error**2))),
                    maximum_error_db=float(np.max(abs(error))),steady_relief_db=float(np.median(curves[label][20000:21000])))
            bites.append(item)
        figdata[key]={k:v[5995:6050] for k,v in curves.items()}
    save('bite_transients.json',bites)
    traces=np.load(BUILD/'comparison_traces.npz');automations=[]
    for key in ['T057','T058','T059','T060','T061']:
        original=traces[key+'_original']
        for start in range(0,60,10):
            # Report active original attenuation only, avoiding digital-silence divisions.
            mask=np.zeros(len(original),dtype=bool);mask[start*100:(start+10)*100]=True
            mask&=original>.1
            automations.append(dict(id=key,interval_s=[start,start+10],active_windows=int(mask.sum()),
                mae_db=float(np.mean(abs(traces[key+'_after'][mask]-original[mask]))) if mask.any() else None))
    save('automation_intervals.json',automations)
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(3,2,figsize=(12,11),layout='constrained')
    for ax,key in zip(axes.flat,['T027','T028','T029','T030']):
        for label,g in figdata[key].items():ax.plot((np.arange(len(g))+.5)-5,g,label=label)
        ax.set(title=key+' Auto BITE / 1 ms RMS',xlabel='ms from onset',ylabel='relief relative to BITE 1 (dB)')
        ax.grid(alpha=.2);ax.legend()
    for ax,key in zip(axes[2],['T059','T061']):
        for label in ['original','after']:
            g=traces[key+'_'+label];ax.plot((np.arange(len(g))+.5)*.01,g,label=label,lw=.8)
        ax.set(title=key+' automation / 10 ms RMS',xlabel='seconds',ylabel='effective attenuation (dB)',ylim=(-1,25))
        ax.grid(alpha=.2);ax.legend()
    fig.savefig(OUT/'comparison.png',dpi=135);plt.close(fig)

def verify():
    inv=json.loads((OUT/'inventory.json').read_text())
    for r in inv:assert sha(ROOT/'renders'/r['file'])==r['sha256'],r['file']
    manifest=json.loads((ROOT/'manifest.json').read_text())
    for r in manifest['files']:assert sha(ROOT/'audio'/r['file'])==r['sha256'],r['file']
    paths=json.loads((BUILD/'after/outputs.json').read_text())
    hashes=json.loads((OUT/'after_output_hashes.json').read_text())
    for k,h in hashes.items():assert sha(Path(paths[k]))==h,k
    for group in [('T002','T035','T036','T037'),('T056','T057')]:
        assert len({hashes[key] for key in group})==1,'Auto manual-time/repeat regression: '+str(group)
    source=ROOT.parent.parent/'Source'
    old=json.loads((ROOT/'analysis_2026-09-23-r1/verification.json').read_text())
    for name,h in old['source_sha256'].items():
        if name.startswith('Source/'):assert sha(BUILD/'baseline-Source'/name[7:])==h,name
    save('verification.json',dict(status='PASS',baseline_commit='4dadce7',dsp_model=8,
        originals_unchanged=len(inv),sources_verified=len(manifest['files']),outputs_verified=len(hashes),
        auto_manual_times_and_repeat_bit_identical=True,
        unchanged_cases=json.loads((OUT/'unchanged.json').read_text()),
        source_sha256={p.relative_to(source.parent).as_posix():sha(p) for p in source.rglob('*') if p.is_file()},
        scripts_sha256={p.name:sha(p) for p in [Path(__file__),ROOT/'fit_auto_bite_2026_09_23.py']},
        acquisition_notes='User confirms P6 values/times exactly follow the plan; old annotated files superseded. Unchanged suspect ALL/MC303 cases remain diagnostic.'))
    print('Integrity PASS',flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--inventory',action='store_true')
    parser.add_argument('--prepare',choices=['before','after']);parser.add_argument('--render',choices=['before','after'])
    parser.add_argument('--original',action='store_true');parser.add_argument('--measure',action='store_true')
    parser.add_argument('--details',action='store_true');parser.add_argument('--verify',action='store_true');args=parser.parse_args()
    if args.inventory:inventory()
    if args.prepare:prepare(args.prepare)
    if args.render:render(args.render)
    if args.original:original()
    if args.measure:measure()
    if args.details:details()
    if args.verify:verify()
