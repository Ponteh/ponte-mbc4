"""Reproduce the September 19 acquisition audit and full-engine comparisons.

python analyse.py --prepare BUILD_DIR
MC2000OriginalPackRender BUILD_DIR/jobs_before.txt
python analyse.py --measure BUILD_DIR --label before
After changing/rebuilding the engine, --prepare BUILD_DIR --label after and repeat.
Audio and intermediate arrays stay local; JSON/PNG summaries can be committed.
"""
from pathlib import Path
import argparse
import hashlib
import json
import sys

import numpy as np
from scipy.io import wavfile
from scipy.optimize import curve_fit
sys.path.insert(0,str(Path(__file__).resolve().parent))
from validate import render_matches, wav_info

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'analysis_2026-09-19'
PLAN = json.loads((ROOT / 'render_plan.json').read_text(encoding='utf-8'))
# Automation is silent, noise neutral T023 duplicates the wrong source,
# BITE/manual/repeat conditions need confirmation. Never use these for fitting.
EXCLUDED = {23,24,27,28,29,30,35,36,37,47}


def audio(path):
    rate, x = wavfile.read(path)
    if x.dtype.kind == 'i': x = x.astype(np.float64) / 2**(8*x.dtype.itemsize-1)
    return rate, x.astype(np.float64)


def rms(x, n=240):
    return np.sqrt(np.mean(x[:len(x)//n*n].reshape(-1,n,2)**2,axis=(1,2)))


def reduction(x, ref, n=240):
    a,b=rms(x,n),rms(ref,n)
    return 20*np.log10(np.maximum(b,1e-15)/np.maximum(a,1e-15)), b


def candidates():
    return {r['id']: render_matches(ROOT/'renders',r) for r in PLAN}


def audit():
    OUT.mkdir(exist_ok=True)
    inventory=[]
    for p in sorted((ROOT/'renders').glob('*.wav')):
        raw=p.read_bytes()
        inventory.append(dict(file=p.name,sha256=hashlib.sha256(raw).hexdigest(),**wav_info(raw)))
    (OUT/'inventory.json').write_text(json.dumps(inventory,indent=2)+'\n',encoding='utf-8')
    pairs=[]
    for i,j in [(2,37),(27,28),(27,35),(27,36),(27,37),(29,30),(35,36),(21,23)]:
        _,a=audio(ROOT/'renders'/f'T{i:03}.wav');_,b=audio(ROOT/'renders'/f'T{j:03}.wav')
        error=np.sum((a-b)**2)
        pairs.append(dict(first=i,second=j,identical=bool(np.array_equal(a,b)),
            max_abs_error=float(np.max(np.abs(a-b))),
            relative_error_db=None if error==0 else float(10*np.log10(error/np.sum(a*a)))))
    fits=[]
    for i,ref,slope in [(2,1,.5),(10,9,.5),(18,17,.5),(20,19,.5),(31,1,.75),(32,1,.5),(33,1,.5),(34,1,.5)]:
        _,a=audio(ROOT/'renders'/f'T{i:03}.wav');_,b=audio(ROOT/'renders'/f'T{ref:03}.wav')
        # Least-squares scalar gain in 1 ms windows of the same filtered carrier.
        a=a[:,0].reshape(-1,48);b=b[:,0].reshape(-1,48)
        gain=np.sum(a*b,axis=1)/np.maximum(np.sum(b*b,axis=1),1e-30)
        gr=-20*np.log10(np.maximum(gain,1e-12));t=(np.arange(len(gr))+.5)/1000
        for end in [22,33,43]:
            m=(t>end+.025)&(t<end+.9)&(gr>.03)
            if np.count_nonzero(m)<5:continue
            def linear_decay(t,amplitude,tau):
                return 20*slope*np.log10(1+amplitude*np.exp(-t/tau))
            pars,_=curve_fit(linear_decay,t[m]-end,gr[m],p0=[10,.1],bounds=([0,.001],[1000,10]))
            fits.append(dict(id=f'T{i:03}',step_end_s=end,slope=slope,amplitude=float(pars[0]),
                tau_s=float(pars[1]),rmse_db=float(np.sqrt(np.mean((linear_decay(t[m]-end,*pars)-gr[m])**2)))))
    result=dict(null_pairs=pairs,release_fits=fits,
        caution='Fit describes observed audio, not proprietary implementation. Positive knee differs; overrides and silent files excluded.')
    (OUT/'original_metrics.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('Inventoried',len(inventory),'renders; measured',len(fits),'release segments.')


def prepare(folder,label):
    folder.mkdir(parents=True,exist_ok=True)
    jobs=[]; selection=[]; paths=candidates()
    for r in PLAN:
        i=int(r['id'][1:])
        if len(paths[r['id']])!=1 or i>=55 or i in EXCLUDED:continue
        if r['sidechain'] or r['automation']:continue
        src=folder/(r['source']+'.f32')
        if not src.exists():
            rate,x=audio(ROOT/'audio'/r['source']);assert rate==r['sample_rate']
            x.astype('<f4').tofile(src)
        dest=folder/(r['id']+'_'+label+'.f32')
        line=[json.dumps(str(src.resolve()).replace('\\','/')),json.dumps(str(dest.resolve()).replace('\\','/')),
              r['sample_rate'],len(r['IN']),r['input_db'],r['output_db'],*r['crossover_hz']]
        for b in r['band_settings']:
            line += [int(r['IN'][b['band']-1]),int(b['band'] in r['SOLO']),b['gain_db'],b['threshold_db'],
                     b['ratio'],b['knee'],b['bite'],b['attack_ms'],b['release_ms'],{'R1':0,'R2':1,'AUTO':2}[b['mode']]]
        assert len(r['IN'])==4 and not r['phase_invert']
        jobs.append(' '.join(map(str,line)));selection.append(r['id'])
    (folder/f'jobs_{label}.txt').write_text('\n'.join(jobs)+'\n',encoding='utf-8')
    print('Prepared',len(selection),'full engine renders:',','.join(selection))


def measure(folder,label):
    rows=[];paths=candidates();traces={}
    for r in PLAN:
        i=r['id']; ref=r['neutral_reference_id']
        output=folder/(i+'_'+label+'.f32');neutral=folder/(ref+'_'+label+'.f32')
        if not output.exists() or not neutral.exists():continue
        _,orig=audio(paths[i][0]);_,orig0=audio(paths[ref][0])
        current=np.fromfile(output,dtype='<f4').astype(float).reshape(-1,2)
        current0=np.fromfile(neutral,dtype='<f4').astype(float).reshape(-1,2)
        assert current.shape==orig.shape==current0.shape==orig0.shape
        go,level=reduction(orig,orig0);gc,lc=reduction(current,current0)
        valid=(level>10**(-65/20))&(lc>10**(-65/20))
        difference=gc[valid]-go[valid]
        active=valid & ((go>.1)|(gc>.1))
        active_error=gc[active]-go[active]
        # B0 checks are absolute output levels; compressed checks remove each
        # engine's own neutral crossover response, avoiding false GR estimates.
        level_error=20*np.log10(np.maximum(lc[valid],1e-15)/level[valid])
        rows.append(dict(id=i,profile=r['profile'],source=r['source'],valid_windows=int(valid.sum()),
            reduction_mae_db=float(np.mean(abs(difference))),reduction_rmse_db=float(np.sqrt(np.mean(difference**2))),
            reduction_p95_abs_db=float(np.quantile(abs(difference),.95)),
            active_windows=int(active.sum()),
            active_reduction_mae_db=float(np.mean(abs(active_error))) if active.any() else 0.0,
            active_reduction_p95_abs_db=float(np.quantile(abs(active_error),.95)) if active.any() else 0.0,
            neutral_level_mae_db=float(np.mean(abs(level_error)))))
        if i in ('T002','T010','T014','T026'):
            old_path=folder/(i+'_before.f32')
            previous=None
            if label=='after' and old_path.exists():
                old=np.fromfile(old_path,dtype='<f4').astype(float).reshape(-1,2)
                previous,_=reduction(old,current0)
            traces[i]=(go,gc,valid,previous)
    OUT.mkdir(exist_ok=True)
    (OUT/f'comparison_{label}.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(len(traces),1,figsize=(12,9),layout='constrained')
    for ax,(i,(go,gc,valid,previous)) in zip(np.atleast_1d(axes),traces.items()):
        t=(np.arange(len(go))+.5)*.005
        ax.plot(t,np.where(valid,go,np.nan),label='McDSP original',lw=1)
        ax.plot(t,np.where(valid,gc,np.nan),label='Ponte '+label,lw=.8,alpha=.85)
        if previous is not None:
            ax.plot(t,np.where(valid,previous,np.nan),label='Ponte before',lw=.8,alpha=.7)
            ax.set_xlim({'T002':(5.98,7.1),'T010':(21.98,22.9),'T014':(11.98,13),'T026':(39,44)}[i])
        ax.set(title=i+' effective attenuation',ylabel='dB',xlabel='seconds');ax.legend();ax.grid(alpha=.2)
    fig.savefig(OUT/f'comparison_{label}.png',dpi=130);plt.close(fig)
    for row in rows:
        if row['profile']!='B0':print(row['id'],row['profile'],'MAE',round(row['reduction_mae_db'],4),'p95',round(row['reduction_p95_abs_db'],4))


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--audit',action='store_true');ap.add_argument('--prepare',type=Path)
    ap.add_argument('--measure',type=Path);ap.add_argument('--label',default='before');args=ap.parse_args()
    if args.audit:audit()
    if args.prepare:prepare(args.prepare,args.label)
    if args.measure:measure(args.measure,args.label)
