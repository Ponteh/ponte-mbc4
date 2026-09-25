"""Reproducible follow-up of user validation captures; originals remain untouched."""
from pathlib import Path
import argparse, hashlib, json, re, subprocess, shutil
import math
import numpy as np
import imageio_ffmpeg
from scipy.io import wavfile
from scipy.signal import resample_poly, correlate
from scipy.optimize import curve_fit, least_squares
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'analysis_2026-09-25'
WORK = ROOT.parents[3] / 'build/mc2000-validation-2026-09-25'
FF = imageio_ffmpeg.get_ffmpeg_exe()

def save(name, data):
    OUT.mkdir(exist_ok=True)
    (OUT/name).write_text(json.dumps(data, indent=2, allow_nan=False)+'\n', encoding='utf-8')

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def rois(stem):
    if 'PONTE' in stem:
        r=[('main',1420,185,230,'yellow'),('main_r',1420,202,230,'yellow')]
        for b in range(4):
            for label,dy in [('in',0),('out',22),('gr',44)]:
                r.append((f'b{b+1}_{label}',1294,489+110*b+dy,348,'red' if label=='gr' else ['ochre','green','orange','purple'][b]))
        if 'R500' in stem:
            r=[(name,x-8,y+1,w,c) for name,x,y,w,c in r]
    else:
        x,y=(539,131) if 'METER16' in stem else (530,146) if 'METER17' in stem else (527,129)
        r=[('main',x+279,y+273,583,'yellow'),('main_r',x+279,y+288,583,'yellow')]
        for b,dy in enumerate([0,99,197,296]):
            # IN/OUT below are selector labels, not separate meter rows.
            # In these captures IN is selected; the two strips are L/R input.
            for label,yy in [('in',409),('in_r',416),('gr',386)]:
                r.append((f'b{b+1}_{label}',x+830,y+yy+dy,234,'yellow'))
    r.append(('play',786,57,16,'green'))
    return r

def colour_mask(a,c):
    r,g,b=a.astype(float).transpose(2,0,1)
    return {'yellow':(r>160)&(g>155)&(b<150),
            'green':(g>120)&(g>r*1.25)&(g>b*1.15),
            'red':(r>170)&(r>g*1.5)&(r>b*1.25),
            'ochre':(r>180)&(g>140)&(g>b*1.5),
            'orange':(r>180)&(r>g*1.3)&(g>b*1.3),
            'purple':(r>120)&(b>170)&(b>g*1.3)}[c]

def extract():
    for p in sorted((ROOT/'renders').glob('METER*.mp4')):
        dest=WORK/(p.stem+'_pixels.npz')
        if dest.exists():
            # Coordinates/semantics are recorded even when pixels are cached.
            a=np.load(dest)['values'];times=a[:,0];rs=rois(p.stem)
            np.savetxt(OUT/(p.stem+'_pixels.csv'),a,delimiter=',',header='time_s,'+','.join(r[0] for r in rs),comments='',fmt='%.6f')
            save(p.stem+'_trace.json',dict(rois=rs,frames=len(times),min_dt=float(np.min(np.diff(times))),max_dt=float(np.max(np.diff(times)))))
            continue
        rs=rois(p.stem);w=600;h=3*len(rs)
        filters=['[0:v]format=rgb24,split='+str(len(rs))+''.join(f'[s{i}]' for i in range(len(rs)))]
        for i,(_,x,y,rw,c) in enumerate(rs):filters.append(f'[s{i}]crop={rw}:3:{x}:{y}:exact=1,pad={w}:3:0:0:black[r{i}]')
        filters.append(''.join(f'[r{i}]' for i in range(len(rs)))+f'vstack=inputs={len(rs)},format=rgb24,showinfo[v]')
        logpath=WORK/(p.stem+'_frames.log');values=[]
        with logpath.open('w') as log:
            proc=subprocess.Popen([FF,'-hide_banner','-nostats','-threads','1','-i',str(p),'-filter_complex',';'.join(filters),'-map','[v]','-an','-fps_mode','passthrough','-f','rawvideo','-pix_fmt','rgb24','pipe:1'],stdout=subprocess.PIPE,stderr=log)
            while True:
                raw=proc.stdout.read(w*h*3)
                if not raw:break
                assert len(raw)==w*h*3
                frame=np.frombuffer(raw,np.uint8).reshape(h,w,3);row=[]
                for i,(name,x,y,rw,c) in enumerate(rs):
                    pixels=np.flatnonzero(colour_mask(frame[i*3:i*3+3,:rw],c).sum(axis=0)>=2)
                    row.append(int((rw-pixels[0]) if name.endswith('_gr') and 'ORIG' in p.stem else pixels[-1]+1) if len(pixels) else 0)
                values.append(row)
            assert proc.wait()==0,logpath.read_text()[-2000:]
        times=np.array([float(t) for t in re.findall(r'\bn:\s*\d+\s+pts:\s*-?\d+\s+pts_time:([\d.e+-]+)',logpath.read_text())])
        assert len(times)==len(values)
        a=np.c_[times,values];np.savez_compressed(dest,values=a)
        np.savetxt(OUT/(p.stem+'_pixels.csv'),a,delimiter=',',header='time_s,'+','.join(r[0] for r in rs),comments='',fmt='%.6f')
        save(p.stem+'_trace.json',dict(rois=rs,frames=len(times),min_dt=float(np.min(np.diff(times))),max_dt=float(np.max(np.diff(times)))))
        print(p.name,len(times),'frames',flush=True)

def audio_checks():
    exe=ROOT.parents[3]/'build/MC2000-bite-2026-09-23/Release/MC2000OriginalPackRender.exe'
    # Reproduce the prior envelope alignment (-50 ms), verified again below
    # with sample-level correlations at five independent voice positions.
    mp3=ROOT/'audio/GAFITA Y CHOMBA_Vocal.mp3'
    raw=subprocess.run([FF,'-v','error','-i',str(mp3),'-ar','48000','-ac','2','-f','f32le','pipe:1'],capture_output=True,check=True).stdout
    voice=np.frombuffer(raw,dtype='<f4').reshape(-1,2)
    np.pad(voice,((2400,0),(0,0)))[:2880000].tofile(WORK/'voice_input.f32')
    jobs=[]
    def job(source,dest,fs,bands,ratio,release,mode,solo):
        fields=[json.dumps(source.as_posix()),json.dumps(dest.as_posix()),fs,bands,0,0,100,785,10000]
        for b in range(4):fields += [1,int(b in solo),0,-27.5,ratio,0,1,2.5,release,mode]
        jobs.append(' '.join(map(str,fields)))
    for release in [0,250,500]:
        job(WORK/'voice_input.f32',WORK/f'voice_{release}.f32',48000,4,2 if release else 1,release or 250,0,[1,2])
    fs,x=wavfile.read(ROOT/'audio/08_MULTITONE_48000Hz_60s.wav')
    x=x.astype(float)/2**31 if x.dtype.kind=='i' else x.astype(float)
    for rate in [48000,192000]:
        src=WORK/f'multitone_{rate}.f32'
        (resample_poly(x,4,1) if rate==192000 else x).astype('<f4').tofile(src)
        for ratio in [1,2]:job(src,WORK/f'mc303_{rate}_{ratio}.f32',rate,3,ratio,250,2,[])
    (WORK/'audio_jobs.txt').write_text('\n'.join(jobs)+'\n')
    subprocess.run([str(exe),str(WORK/'audio_jobs.txt')],check=True)
    result={'renderer_sha256':sha(exe),'voice_source_sha256':sha(mp3),'aligned_voice_sha256':sha(WORK/'voice_input.f32'),'T045':{},'voice':{}}
    originals=[]
    for key in ['T044','T045']:
        fs,o=wavfile.read(ROOT/'renders'/f'{key}.wav');assert fs==192000
        originals.append(resample_poly(o.astype(float),1,4))
    rms=lambda v:np.sqrt(np.mean(v[:len(v)//480*480].reshape(-1,480,2)**2,axis=(1,2)))
    gr=lambda a,b:20*np.log10(np.maximum(rms(a),1e-15)/np.maximum(rms(b),1e-15))
    go=gr(*originals);valid=rms(originals[0])>10**(-65/20)
    for rate in [48000,192000]:
        pair=[np.fromfile(WORK/f'mc303_{rate}_{r}.f32',dtype='<f4').reshape(-1,2).astype(float) for r in [1,2]]
        if rate==192000:pair=[resample_poly(a,1,4) for a in pair]
        gp=gr(*pair);err=gp-go
        m=valid&(go>.1)
        result['T045'][str(rate)]=dict(mae_all_db=float(abs(err[valid]).mean()),mae_active_db=float(abs(err[m]).mean()),p95_db=float(np.quantile(abs(err[valid]),.95)))
    tones=[]
    for label,pair in [('original_resampled',originals),('ponte_192000',pair)]:
        for start,stop in [(9,11),(37.5,40.5)]:
            a,b=[v[int(start*48000):int(stop*48000),0] for v in pair]
            t=np.arange(len(a))/48000
            for hz in [50,315,2000,14000]:
                carrier=np.exp(-2j*np.pi*hz*t)
                tones.append(dict(model=label,start_s=start,hz=hz,gr_db=float(20*np.log10(abs(a@carrier)/abs(b@carrier)))))
    result['T045']['tones']=tones
    for release in [250,500]:
        path=next((ROOT/'renders').glob(f'*{release}rel,2-3.wav'));fs,o=wavfile.read(path);o=o.astype(float)
        p=np.fromfile(WORK/f'voice_{release}.f32',dtype='<f4').reshape(-1,2).astype(float)
        lags=[]
        for second in [5,15,25,35,45]:
            a=o[second*48000:(second+1)*48000,0];b=p[second*48000:(second+1)*48000,0]
            c=correlate(a,b,mode='full',method='fft');mid=len(b)-1;lag=int(np.argmax(c[mid-480:mid+481]))-480
            lags.append(dict(at_s=second,lag_samples=lag,correlation=float(c[mid+lag]/np.sqrt(np.sum(a*a)*np.sum(b*b)))))
        lag=int(np.median([r['lag_samples'] for r in lags]));assert max(abs(r['lag_samples']-lag) for r in lags)<=4
        align=lambda v:np.pad(v,((max(lag,0),max(-lag,0)),(0,0)))[max(-lag,0):len(v)+max(-lag,0)]
        p=align(p);n=align(np.fromfile(WORK/'voice_0.f32',dtype='<f4').reshape(-1,2).astype(float))
        gp=gr(n,p);go=gr(n,o);m=rms(n)>10**(-60/20);m[:100]=False;err=gp-go
        result['voice'][str(release)]=dict(alignment=lags,mae_db=float(abs(err[m]).mean()),bias_db=float(err[m].mean()),p95_db=float(np.quantile(abs(err[m]),.95)),routing='all IN; SOLO 2/3',limitation='MP3 reference and Ponte neutral; original neutral/dry missing')
    save('audio_comparison.json',result);print(json.dumps(result,indent=2),flush=True)

def measure_levels():
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    result={};fig,axes=plt.subplots(3,1,figsize=(12,10),layout='constrained')
    for brand in ['ORIG','PONTE']:
        stem=f'METER16_{brand}_R250_take1';rs=rois(stem)
        a=np.load(WORK/(stem+'_pixels.npz'))['values'];t=a[:,0];play=t[a[:,-1]>0][0]
        nominal=np.array([6,9,12,32,35,38]);obs=[]
        for e in nominal:
            m=(t>play+e-.5)&(t<play+e+.7);tt=t[m];v=a[m,1]
            lo=np.median(v[:6]);hi=np.median(v[-6:]);obs.append(tt[np.flatnonzero(v>=lo+.5*(hi-lo))[0]])
        slope,offset=np.polyfit(nominal,obs,1);rel=(t-offset)/slope
        rec=dict(play_start_s=float(play),event_offset_s=float(offset),time_scale=float(slope),rising_event_residual_max_ms=float(max(abs(np.array(obs)-(offset+slope*nominal)))*1000),channels={})
        for chan,groups in [('main',[3,29]),('b2_in',[3]),('b2_in_r' if brand=='ORIG' else 'b2_out',[3]),('b3_in',[29]),('b3_in_r' if brand=='ORIG' else 'b3_out',[29])]:
            idx=next(i for i,r in enumerate(rs) if r[0]==chan);w=rs[idx][3];px=a[:,idx+1]/w
            dbs=[];ys=[]
            for group in groups:
                for k,db in enumerate([-36,-24,-12,-6,-12,-24,-36]):
                    m=(rel>group+k*3+1.3)&(rel<group+k*3+2.6);dbs.append(db);ys.append(np.median(px[m]))
            if brand=='ORIG':
                fun=lambda db,a,k,b:a*10**(db/k)+b
                pars,_=curve_fit(fun,dbs,ys,p0=[1.015,40.24,-.015],bounds=([.5,25,-.15],[1.5,60,.15]))
                db=pars[1]*np.log10(np.maximum((px-pars[2])/pars[0],1e-8));cal=dict(a=float(pars[0]),k=float(pars[1]),b=float(pars[2]))
            else:
                pars=np.polyfit(dbs,ys,1);fun=lambda db,s,b:s*np.array(db)+b
                db=(px-pars[1])/pars[0];cal=dict(slope=float(pars[0]),intercept=float(pars[1]))
            falls=[];rises=[]
            for group in groups:
                for delta,event in [(6,group+12),(12,group+15),(12,group+18)]:
                    hi=np.median(db[(rel>event-.6)&(rel<event-.2)]);lo=np.median(db[(rel>event+1.5)&(rel<event+2.5)])
                    m=(rel>event-.1)&(rel<event+2.5);tt=rel[m];v=db[m]
                    cross=[float(tt[np.flatnonzero(v<=hi-f*(hi-lo))[0]]) for f in [.1,.9]]
                    falls.append(dict(event_s=event,drop_db=delta,t10_s=cross[0]-event,t90_s=cross[1]-event,interval_s=cross[1]-cross[0]))
                for event in [group+3,group+6,group+9]:
                    lo=np.median(db[(rel>event-.5)&(rel<event-.2)]);hi=np.median(db[(rel>event+.5)&(rel<event+1)])
                    m=(rel>event-.15)&(rel<event+.6);tt=rel[m];v=db[m]
                    cross=[float(tt[np.flatnonzero(v>=lo+f*(hi-lo))[0]]) for f in [.1,.9]]
                    rises.append(dict(event_s=event,interval_s=cross[1]-cross[0]))
            rec['channels'][chan]=dict(calibration=cal,calibration_rmse_pixels=float(np.sqrt(np.mean((fun(np.array(dbs),*pars)-ys)**2))*w),falls=falls,rises=rises)
            if chan in ['main','b2_in','b3_in']:
                ax=axes[['main','b2_in','b3_in'].index(chan)];ax.plot(rel,db,label=brand,linewidth=.8);ax.set(title=chan,xlim=(2,53),ylim=(-60,0),ylabel='calibrated dB');ax.grid(alpha=.2);ax.legend()
        result[brand]=rec
    axes[-1].set_xlabel('stimulus seconds (relative visual alignment)');fig.savefig(OUT/'level_meter_comparison.png',dpi=130);plt.close(fig)
    save('level_measurements.json',result)
    for brand,r in result.items():
        print(brand,'clock',r['time_scale'],flush=True)
        for chan,c in r['channels'].items():print(chan,'calibration',c['calibration_rmse_pixels'],'falls',[(f['drop_db'],round(f['interval_s'],3)) for f in c['falls']],'rises',[round(f['interval_s'],3) for f in c['rises']],flush=True)

def measure_bursts():
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    events=next(r for r in json.loads((ROOT/'manifest.json').read_text())['files'] if r['file'].startswith('17_'))['events']
    bursts=[r for r in events if r['label']=='meter_burst']
    results={};fig,axes=plt.subplots(4,2,figsize=(14,12),layout='constrained')
    for release in [250,500]:
        for brand in ['ORIG','PONTE']:
            stem=f'METER17_{brand}_R{release}_take1';rs=rois(stem)
            a=np.load(WORK/(stem+'_pixels.npz'))['values'];t=a[:,0];play=t[a[:,-1]>0][0]
            rec=dict(play_start_s=float(play),events=[])
            for band,hz in [(2,315),(3,2000)]:
                def get(label):
                    idx=next(i for i,r in enumerate(rs) if r[0]==f'b{band}_{label}')
                    px=a[:,idx+1]/rs[idx][3]
                    if brand=='ORIG':
                        if label=='gr':px=1-px
                        db=40.24*np.log10(np.maximum((px+.015)/1.015,1e-6))
                        return np.maximum(0,-db) if label=='gr' else db
                    return px*60 if label=='gr' else px*60-60
                inp=get('in');out=get('in_r' if brand=='ORIG' else 'out');gr=get('gr');time=t-play
                for row,(label,v) in enumerate([('in',inp),('out',out),('gr',gr),('in-out',inp-out)]):
                    if brand=='ORIG' and row in [1,3]:continue
                    ax=axes[row,int(release==500)];ax.plot(time,v,label=f'{brand} B{band}',lw=.8);ax.set(title=f'{label} R{release}',xlim=(2,52),ylim=(0,14) if row>=2 else (-60,0));ax.grid(alpha=.2);ax.legend(fontsize=8)
                for e in bursts:
                    if e['carrier_hz']!=hz:continue
                    start=e['start_s'];end=e['end_s'];m=(time>start-.1)&(time<end+.15)
                    peak=float(np.max(gr[m]));peak_in=float(np.max(inp[m]));peak_out=float(np.max(out[m]));cross=[]
                    peak_time=time[m][np.argmax(gr[m])]
                    next_start=min([b['start_s'] for b in bursts if b['carrier_hz']==hz and b['start_s']>start]+[60])
                    for f in [.9,.1]:
                        after=(time>=max(end,peak_time))&(time<min(end+3,next_start))
                        indices=np.flatnonzero(gr[after]<=f*peak)
                        cross.append(float(time[after][indices[0]]-end) if len(indices) else None)
                    rec['events'].append(dict(band=band,start_s=start,duration_s=end-start,gr_peak_db=peak,input_peak_db=peak_in,
                        **{'input_right_peak_db' if brand=='ORIG' else 'output_peak_db':peak_out},
                        return90_s=cross[0],return10_s=cross[1],return90_10_s=cross[1]-cross[0] if all(v is not None for v in cross) else None))
            results[stem]=rec
    fig.savefig(OUT/'burst_meter_comparison.png',dpi=140);plt.close(fig);save('burst_measurements.json',results)
    for stem,r in results.items():print(stem,[(e['band'],round(e['duration_s'],3),round(e['gr_peak_db'],2),round(e['return90_10_s'],3) if e['return90_10_s'] is not None else None) for e in r['events']],flush=True)

def fit_gr():
    fs,x=wavfile.read(ROOT/'audio/17_METER_BURSTS_48000Hz_60s.wav')
    assert fs==48000
    if x.dtype.kind=='i':x=x.astype(float)/2**31
    src=WORK/'bursts.f32';x.astype('<f4').tofile(src)
    exe=WORK/'probe/Release/MeterValidationProbe.exe'
    traces={}
    for release in [250,500]:
        dest=WORK/f'raw_meters_{release}.csv'
        if not dest.exists():subprocess.run([str(exe),str(src),str(dest),str(release)],check=True)
        traces[release]=np.loadtxt(dest,delimiter=',',skiprows=1)
    data=[]
    for release in [250,500]:
        stem=f'METER17_ORIG_R{release}_take1';a=np.load(WORK/(stem+'_pixels.npz'))['values'];rs=rois(stem)
        t=a[:,0];play=t[a[:,-1]>0][0];time=t-play
        for band,hz in [(2,315),(3,2000)]:
            idx=next(i for i,r in enumerate(rs) if r[0]==f'b{band}_gr')
            px=a[:,idx+1]/234;gr=np.maximum(0,-40.24*np.log10(np.maximum((1-px+.015)/1.015,1e-6)))
            # Each constant displayed segment corresponds to one GUI refresh.
            changed=np.r_[True,np.diff(gr)!=0];mask=changed&(time>2)&(time<53)
            tt=time[mask];observed=gr[mask]
            raw=traces[release][:,3 if band==2 else 6]
            data.append((release,band,tt,observed,raw))
    # Model the peak mailbox at a 30 Hz GUI, preserving short source peaks.
    # Offset is a nuisance parameter for capture scheduling, not audio latency.
    def predict(raw,attack,decay,phase):
        ticks=np.arange(phase,60,1/30);prev=0;d=0;vals=[]
        for tick in ticks:
            end=min(len(raw),max(prev+1,int(round(tick*1000))))
            peak=float(np.max(raw[prev:end])) if end>prev else 0
            tau=attack if peak>d else decay
            d=peak if tau<=1e-7 else peak+(d-peak)*math.exp(-1/30/tau)
            vals.append(d);prev=end
        return ticks,np.array(vals)
    def residual(pars,subset):
        attack,decay,offset=pars;out=[]
        for release,band,t,y,raw in subset:
            tt,v=predict(raw,attack,decay,1/30)
            out.extend(np.interp(t-offset,tt,v)-y)
        return np.array(out)
    training=[r for r in data if r[0]==250]
    fit=least_squares(lambda p:residual(p,training),[.04,.15,.015],bounds=([.001,.02,-.1],[.2,.5,.15]),diff_step=.005)
    result=dict(training='R250, both bands; R500 held out',attack_s=float(fit.x[0]),decay_s=float(fit.x[1]),visual_offset_s=float(fit.x[2]),models={})
    measured=json.loads((OUT/'burst_measurements.json').read_text())
    for name,pars in [('before',[0,.15,fit.x[2]]),('fit',fit.x),('implemented',[.045,.090,fit.x[2]])]:
        metrics={}
        for release in [250,500]:
            err=residual(pars,[r for r in data if r[0]==release])
            metrics[str(release)]=dict(mae_db=float(abs(err).mean()),rmse_db=float(np.sqrt(np.mean(err**2))),p95_db=float(np.quantile(abs(err),.95)))
            peak_errors=[]
            for _,band,t,y,raw in [r for r in data if r[0]==release]:
                tt,v=predict(raw,pars[0],pars[1],1/30)
                for e in measured[f'METER17_ORIG_R{release}_take1']['events']:
                    if e['band']!=band:continue
                    m=(tt>=e['start_s'])&(tt<e['start_s']+e['duration_s']+.2)
                    peak_errors.append(float(np.max(v[m])-e['gr_peak_db']))
            metrics[str(release)]['peak_mae_db']=float(np.mean(np.abs(peak_errors)))
        result['models'][name]=metrics
    save('gr_fit.json',result);print(json.dumps(result,indent=2),flush=True)
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(2,2,figsize=(13,8),layout='constrained')
    for ax,(release,band,t,y,raw) in zip(axes.flat,data):
        ax.plot(t,y,label='Original display',lw=1)
        for name,attack,decay in [('Before',0,.15),('Implemented',.045,.090)]:
            tt,v=predict(raw,attack,decay,1/30);ax.plot(tt+fit.x[2],v,label=name,lw=.8)
        ax.set(title=f'R{release} B{band}',xlim=(2.9,10) if band==2 else (28.9,36),ylim=(0,12),ylabel='GR dB');ax.grid(alpha=.2);ax.legend()
    fig.savefig(OUT/'gr_candidate.png',dpi=130);plt.close(fig)

def build_probe():
    WORK.mkdir(parents=True,exist_ok=True)
    product=ROOT.parent.parent
    lines=['cmake_minimum_required(VERSION 3.24)','project(MeterValidationProbe LANGUAGES CXX)','set(CMAKE_CXX_STANDARD 20)',
           f'add_executable(MeterValidationProbe "{product.as_posix()}/Research/MeterValidationProbe.cpp" "{product.as_posix()}/Source/DSP/MultiBandCompressor.cpp" "{product.as_posix()}/Source/DSP/CrossoverNetwork.cpp")',
           f'target_include_directories(MeterValidationProbe PRIVATE "{product.as_posix()}/Source")']
    (WORK/'CMakeLists.txt').write_text('\n'.join(lines)+'\n')
    cmake=shutil.which('cmake') or 'C:/cmake-3.30.1-windows-x86_64/bin/cmake.exe'
    subprocess.run([cmake,'-S',str(WORK),'-B',str(WORK/'probe'),'-G','Visual Studio 17 2022','-A','x64'],check=True)
    subprocess.run([cmake,'--build',str(WORK/'probe'),'--config','Release','--parallel','2'],check=True)

def inventory():
    WORK.mkdir(parents=True, exist_ok=True)
    records=[]
    paths=sorted(list((ROOT/'renders').glob('METER*.mp4'))+
                 list((ROOT/'renders').glob('*2-3.*'))+
                 [ROOT/'renders/T044.wav', ROOT/'renders/T045.wav'])
    for p in paths:
        if p.suffix not in ['.wav','.mp4']: continue
        rec=dict(file=p.name, sha256=sha(p), bytes=p.stat().st_size)
        if p.suffix=='.wav':
            fs,x=wavfile.read(p)
            rec.update(sample_rate=fs, channels=x.shape[1] if x.ndim==2 else 1,
                       samples=len(x), seconds=len(x)/fs, dtype=str(x.dtype),
                       peak=float(np.max(abs(x))), finite=bool(np.isfinite(x).all()))
        else:
            reader=imageio_ffmpeg.read_frames(str(p)); meta=next(reader);reader.close()
            rec.update(meta)
            raw=subprocess.run([FF,'-v','error','-i',str(p),'-vn','-ac','1','-ar','48000','-f','f32le','pipe:1'],capture_output=True,check=True).stdout
            audio=np.frombuffer(raw,dtype='<f4')
            rec.update(audio_peak=float(np.max(abs(audio))) if len(audio) else None,
                       audio_samples=len(audio),audio_nonzero=int(np.count_nonzero(audio)))
            audio.tofile(WORK/(p.stem+'_audio.f32'))
            for sec in [10,25,45]:
                subprocess.run([FF,'-v','error','-ss',str(sec),'-i',str(p),'-frames:v','1','-y',str(WORK/(p.stem+f'_{sec}.png'))],check=True)
        records.append(rec)
        print(rec,flush=True)
    save('inventory.json',records)
    vids=[r for r in records if r['file'].endswith('.mp4')]
    sheet=Image.new('RGB',(960*2,565*((len(vids)+1)//2)), '#303030')
    d=ImageDraw.Draw(sheet)
    for i,r in enumerate(vids):
        p=Path(r['file']); im=Image.open(WORK/(p.stem+'_25.png'));im.thumbnail((960,540))
        x=(i%2)*960;y=(i//2)*565
        sheet.paste(im,(x,y+25));d.text((x+5,y+5),p.name,fill='white')
    sheet.save(OUT/'capture_overview.jpg',quality=90)

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    for flag in ['inventory','extract','audio','levels','bursts','fit-gr','build-probe']:ap.add_argument('--'+flag,action='store_true')
    args=ap.parse_args()
    if args.build_probe:build_probe()
    if args.inventory:inventory()
    if args.extract:extract()
    if args.audio:audio_checks()
    if args.levels:measure_levels()
    if args.bursts:measure_bursts()
    if args.fit_gr:fit_gr()
