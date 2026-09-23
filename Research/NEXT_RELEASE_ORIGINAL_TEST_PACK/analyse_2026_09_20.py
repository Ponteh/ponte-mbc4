"""Audit the September 20 delivery against frozen DSP_MODEL_6, without fitting it.

--prepare creates only jobs missing from the hash-verified previous DSP renders.
Run the three jobs_0/1/2.txt with MC2000OriginalPackRender, then --measure.
Reports are separate from September 19; original recordings are never changed.
"""
from pathlib import Path
import argparse, hashlib, json, sys
import numpy as np
from scipy.optimize import curve_fit
sys.path.insert(0,str(Path(__file__).resolve().parent))
from analyse import audio
from validate import wav_info

ROOT=Path(__file__).resolve().parent
PRODUCT=ROOT.parent.parent
WORKSPACE=PRODUCT.parents[1]
OUT=ROOT/'analysis_2026-09-20'
BUILD=WORKSPACE/'build/mc2000-auto-2026-09-20'
OLD_BUILD=WORKSPACE/'build/mc2000-auto-2026-09-19'
PLAN=json.loads((ROOT/'render_plan.json').read_text())
OLD=json.loads((ROOT/'analysis_2026-09-19/verification.json').read_text())

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(name,data):
    OUT.mkdir(exist_ok=True);(OUT/name).write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')

def inventory():
    old={r['file']:r for r in json.loads((ROOT/'analysis_2026-09-19/inventory.json').read_text())}
    rows=[]
    for p in sorted((ROOT/'renders').glob('*.wav')):
        raw=p.read_bytes();digest=hashlib.sha256(raw).hexdigest()
        rows.append(dict(file=p.name,sha256=digest,**wav_info(raw),previous='new' if p.name not in old
            else 'unchanged' if digest==old[p.name]['sha256'] else 'replaced'))
    save('inventory.json',rows)

def selected():
    inv={r['file']:r for r in json.loads((OUT/'inventory.json').read_text())}
    chosen={};missing=[];excluded=[]
    for r in PLAN:
        name=r['id']+'.wav'
        if name not in inv:missing.append(r['id']);continue
        info=inv[name];reason=[]
        if info['all_silent']:reason.append('all samples zero')
        if info['sample_rate']!=r['sample_rate']:reason.append('sample rate differs from plan')
        if info['duration_s']!=60:reason.append('duration differs from 60 s')
        if info['channels']!=2:reason.append('not stereo')
        if reason:excluded.append(dict(id=r['id'],reason=reason));continue
        chosen[r['id']]=r
    save('selection.json',dict(selected=list(chosen),missing=missing,excluded=excluded,
        policy='Exact Txxx.wav selects the new delivery. Annotated September 19 files are retained as superseded takes.'))
    return chosen

def prepare():
    BUILD.mkdir(exist_ok=True,parents=True);rows=selected();jobs=[];outputs={};reuse=0
    for name,h in OLD['source_sha256'].items():
        # Research scripts may evolve; production DSP must match the frozen model.
        if name.startswith('Source/'):assert sha(PRODUCT/name)==h,name+' production baseline changed'
    exe=WORKSPACE/'build/MC2000-analysis-vs/Release/MC2000OriginalPackRender.exe'
    assert sha(exe)==OLD['local_build']['renderer_sha256']
    known={r['id']:r for r in OLD['renders']}
    sources={r['file']:r for r in json.loads((ROOT/'manifest.json').read_text())['files']}
    checked=set()
    for key,r in rows.items():
        assert r['automation'] is None and r['sidechain'] is None and not r['phase_invert']
        dest=OLD_BUILD/(key+'_after.f32')
        if key in known and dest.exists() and sha(dest)==known[key]['after_sha256']:
            outputs[key]=str(dest);reuse+=1;continue
        src=BUILD/(r['source']+'.f32')
        if r['source'] not in checked:
            assert sha(ROOT/'audio'/r['source'])==sources[r['source']]['sha256']
            rate,x=audio(ROOT/'audio'/r['source']);assert rate==r['sample_rate']
            x.astype('<f4').tofile(src);checked.add(r['source'])
        dest=BUILD/(key+'_model6.f32');outputs[key]=str(dest)
        cross=r['crossover_hz']+[1000,10000][len(r['crossover_hz'])-1:] if len(r['crossover_hz'])<3 else r['crossover_hz']
        assert len(cross)==3 and all(a<b for a,b in zip(cross,cross[1:]))
        fields=[json.dumps(str(src).replace('\\','/')),json.dumps(str(dest).replace('\\','/')),r['sample_rate'],len(r['IN']),r['input_db'],r['output_db'],*cross]
        for i in range(4):
            if i<len(r['band_settings']):
                b=r['band_settings'][i]
                fields.extend([int(r['IN'][i]),int(i+1 in r['SOLO']),b['gain_db'],b['threshold_db'],b['ratio'],b['knee'],b['bite'],b['attack_ms'],b['release_ms'],{'R1':0,'R2':1,'AUTO':2}[b['mode']]])
            else:fields.extend([1,0,0,0,1,0,1,2.5,250,0])
        jobs.append(' '.join(map(str,fields)))
    for i in range(3):(BUILD/f'jobs_{i}.txt').write_text('\n'.join(jobs[i::3])+'\n')
    (BUILD/'outputs.json').write_text(json.dumps(outputs,indent=2)+'\n')
    save('engine_provenance.json',dict(source_commit='5d8bd92709b113a7957db591a609ef3044dd350e',
        renderer_sha256=sha(exe),dsp_model=6,version='0.2.3',reused_verified_renders=reuse,new_jobs=len(jobs),
        source_sha256={k:v for k,v in OLD['source_sha256'].items() if k.startswith('Source/')}))
    print(len(rows),'selected,',reuse,'verified reuses,',len(jobs),'new jobs',flush=True)

def rms(x,rate):
    n=round(rate*.01);return np.sqrt(np.mean(x[:len(x)//n*n].reshape(-1,n,2)**2,axis=(1,2)))
def reduction(x,b,rate):
    a,z=rms(x,rate),rms(b,rate)
    return 20*np.log10(np.maximum(z,1e-15)/np.maximum(a,1e-15)),z
def get_original(key):return audio(ROOT/'renders'/(key+'.wav'))

def measure():
    rows=selected();outputs=json.loads((BUILD/'outputs.json').read_text());result=[];traces={};hashes={}
    for key,r in rows.items():
        if not Path(outputs[key]).exists():raise RuntimeError('Missing '+key)
        rate,o=get_original(key);_,b=get_original(r['neutral_reference_id'])
        x=np.fromfile(outputs[key],dtype='<f4').astype(float).reshape(-1,2)
        b0=np.fromfile(outputs[r['neutral_reference_id']],dtype='<f4').astype(float).reshape(-1,2)
        assert x.shape==o.shape==b.shape==b0.shape
        go,lo=reduction(o,b,rate);gx,lx=reduction(x,b0,rate)
        valid=(lo>10**(-65/20))&(lx>10**(-65/20));active=valid&(go>.1)
        error=gx-go
        def metrics(mask):return dict(count=int(mask.sum()),mae_db=float(np.mean(abs(error[mask]))) if mask.any() else None,
            p95_db=float(np.quantile(abs(error[mask]),.95)) if mask.any() else None,
            maximum_db=float(np.max(abs(error[mask]))) if mask.any() else None)
        result.append(dict(id=key,model=r['model'],rate=rate,profile=r['profile'],all_valid=metrics(valid),
            original_compressing=metrics(active),effective_attenuation=not bool(r['SOLO']) or len(r['SOLO'])>1,
            neutral_level_mae_db=float(np.mean(abs(20*np.log10(np.maximum(lx[valid],1e-15)/lo[valid]))))))
        if r['profile']!='B0':print(key,r['profile'],round(result[-1]['all_valid']['mae_db'],4),flush=True)
        if key in ['T027','T028','T029','T030','T039','T043','T063','T073','T083','T084','T085','T086']:
            traces[key]=(go,gx,valid)
        hashes[key]=sha(Path(outputs[key]))
    save('comparison.json',result);save('engine_output_hashes.json',hashes)
    np.savez_compressed(BUILD/'traces.npz',**{k:np.vstack(v) for k,v in traces.items()})
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(3,2,figsize=(13,10),layout='constrained')
    for ax,key in zip(axes.flat,['T027','T028','T039','T043','T083','T084']):
        go,gx,valid=traces[key];t=(np.arange(len(go))+.5)*.01
        ax.plot(t,np.where(valid,go,np.nan),label='McDSP original',lw=1)
        ax.plot(t,np.where(valid,gx,np.nan),label='Ponte DSP_MODEL_6',lw=1,alpha=.8)
        ax.set(xlim=(5.98,6.15) if key in ['T027','T028'] else (19,23) if key in ['T039','T043'] else (3,19),
               title=key,xlabel='seconds',ylabel='effective attenuation dB');ax.grid(alpha=.2);ax.legend(fontsize=8)
    fig.savefig(OUT/'comparison.png',dpi=130);plt.close(fig)

def original_details():
    # Same-source comparisons. No time shifting and no fit to the new engine.
    nulls=[]
    for i,j in [(2,27),(27,28),(10,29),(29,30),(35,36),(36,37),(42,43),(55,56),(21,66),(22,67)]:
        ra,a=get_original(f'T{i:03}');rb,b=get_original(f'T{j:03}');assert ra==rb and a.shape==b.shape
        e=np.sum((a-b)**2);s=np.sum(a*a)
        nulls.append(dict(first=i,second=j,identical=bool(np.array_equal(a,b)),both_silent=bool(s==0 and not np.any(b)),
            max_abs_error=float(np.max(abs(a-b))),relative_error_db=None if e==0 or s==0 else float(10*np.log10(e/s))))
    fits=[]
    for key,ref in [('T039','T038'),('T043','T042')]:
        rate,a=get_original(key);_,b=get_original(ref);n=48
        a=a[:,0].reshape(-1,n);b=b[:,0].reshape(-1,n)
        g=-20*np.log10(np.maximum(np.sum(a*b,axis=1)/np.maximum(np.sum(b*b,axis=1),1e-30),1e-12))
        t=(np.arange(len(g))+.5)/1000
        for end in [22,33,43]:
            m=(t>end+.025)&(t<end+.9)&(g>.03)
            if np.count_nonzero(m)<5:
                fits.append(dict(id=key,end_s=end,status='no positive measurable release segment'))
                continue
            f=lambda t,a,tau:10*np.log10(1+a*np.exp(-t/tau))
            pars,_=curve_fit(f,t[m]-end,g[m],p0=[10,.1],bounds=([0,.001],[1000,10]))
            fits.append(dict(id=key,end_s=end,tau_ms=float(pars[1]*1000),rmse_db=float(np.sqrt(np.mean((f(t[m]-end,*pars)-g[m])**2)))))
    bite=[]
    for base,alt,ref in [('T002','T027','T001'),('T002','T028','T001'),('T010','T029','T009'),('T010','T030','T009')]:
        rate,a=get_original(base);_,b=get_original(alt);_,neutral=get_original(ref)
        # 1 ms RMS windows; peak ratios at near-silence excluded.
        n=48
        energy=lambda x:np.mean(x[:,0].reshape(-1,n)**2,axis=1)
        ea,eb,en=energy(a),energy(b),energy(neutral)
        relief=10*np.log10(np.maximum(eb,1e-30)/np.maximum(ea,1e-30));t=(np.arange(len(relief))+.5)/1000
        m=(t>=6)&(t<6.05)&(en>10**(-45/10))
        bite.append(dict(base=base,variant=alt,max_first_50ms_relief_db=float(np.max(relief[m])),
            time_of_max_s=float(t[m][np.argmax(relief[m])]),steady_relief_db=float(np.median(relief[(t>20)&(t<21)]))))
    save('original_details.json',dict(nulls=nulls,cross_model_auto_release=fits,bite=bite))
    print(json.dumps(dict(fits=fits,bite=bite),indent=2))

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--inventory',action='store_true');ap.add_argument('--prepare',action='store_true')
    ap.add_argument('--measure',action='store_true');ap.add_argument('--original',action='store_true');args=ap.parse_args()
    if args.inventory:inventory()
    if args.prepare:prepare()
    if args.measure:measure()
    if args.original:original_details()
