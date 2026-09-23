"""Native-timestamp meter measurements. No resampling of video or editing audio."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
import numpy as np
import imageio_ffmpeg
from scipy.optimize import curve_fit

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'analysis_2026-09-20'
WORK=ROOT.parents[3]/'build/mc2000-auto-2026-09-20/video'
FF=imageio_ffmpeg.get_ffmpeg_exe()
# Native 1920x1080 coordinates inspected in all four recordings.
ROIS=[('main',951,426,583,'yellow'),('main_r',951,441,583,'yellow'),
      ('b1',1502,563,234,'yellow'),('b2',1502,662,234,'yellow'),
      ('b3',1502,760,234,'yellow'),('b4',1502,860,234,'yellow'),
      ('play',786,56,16,'green')]

def save(name,data):(OUT/name).write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
def mask(a,colour):
    r,g,b=a.astype(float).transpose(2,0,1)
    return (r>150)&(g>155)&(b<145) if colour=='yellow' else (g>95)&(g>r*1.25)&(g>b*1.08)

def extract():
    WORK.mkdir(parents=True,exist_ok=True);inventory={}
    for p in sorted((ROOT/'renders').glob('T08*.mp4')):
        reader=imageio_ffmpeg.read_frames(str(p));metadata=next(reader);reader.close()
        assert tuple(metadata['size'])==(1920,1080)
        filters=['[0:v]format=rgb24,split='+str(len(ROIS))+''.join(f'[s{i}]' for i in range(len(ROIS)))]
        for i,(_,x,y,w,colour) in enumerate(ROIS):filters.append(f'[s{i}]crop={w}:3:{x}:{y}:exact=1,pad=600:3:0:0:black[r{i}]')
        filters.append(''.join(f'[r{i}]' for i in range(len(ROIS)))+f'vstack=inputs={len(ROIS)},format=rgb24,showinfo[v]')
        logpath=WORK/(p.stem+'_frames.log');records=[];size=600*3*len(ROIS)*3
        with logpath.open('w') as log:
            proc=subprocess.Popen([FF,'-hide_banner','-nostats','-threads','1','-i',str(p),'-filter_complex',';'.join(filters),
                '-map','[v]','-an','-fps_mode','passthrough','-f','rawvideo','-pix_fmt','rgb24','pipe:1'],stdout=subprocess.PIPE,stderr=log)
            while True:
                raw=proc.stdout.read(size)
                if not raw:break
                assert len(raw)==size
                frame=np.frombuffer(raw,np.uint8).reshape(3*len(ROIS),600,3);row=[]
                for i,(_,x,y,w,colour) in enumerate(ROIS):
                    active=np.where(np.sum(mask(frame[i*3:(i+1)*3,:w],colour),axis=0)>=2)[0]
                    row.append(int(active[-1]+1) if len(active) else 0)
                records.append(row)
            assert proc.wait()==0,logpath.read_text()[-2000:]
        times=np.array([float(t) for t in re.findall(r'\bn:\s*\d+\s+pts:\s*-?\d+\s+pts_time:([\d.e+-]+)',logpath.read_text())])
        assert len(times)==len(records)
        values=np.c_[times,records];np.savez_compressed(WORK/(p.stem+'_pixels.npz'),values=values)
        np.savetxt(OUT/(p.stem+'_pixels.csv'),values,delimiter=',',header='time_s,'+','.join(r[0] for r in ROIS),comments='',fmt='%.6f')
        raw=subprocess.run([FF,'-v','error','-i',str(p),'-vn','-ac','1','-f','f32le','pipe:1'],capture_output=True,check=True).stdout
        audio=np.frombuffer(raw,np.float32)
        inventory[p.stem]=dict(**metadata,sha256=hashlib.sha256(p.read_bytes()).hexdigest(),frames=len(times),
            min_frame_interval=float(np.min(np.diff(times))),max_frame_interval=float(np.max(np.diff(times))),
            audio_peak=float(np.max(abs(audio))),audio_nonzero_samples=int(np.count_nonzero(audio)),rois=ROIS)
        print(p.name,inventory[p.stem]['frames'],'frames, audio peak',inventory[p.stem]['audio_peak'],flush=True)
        save('video_inventory.json',inventory)

def model_interval(drop):
    ramp=displayed=0.;values=[]
    for i in range(2500):
        dt=.001;target=-drop;fall=min(dt,(ramp-target)/14.3);end=ramp-14.3*fall
        displayed=end+14.3*.130+(displayed-ramp-14.3*.130)*np.exp(-fall/.130);ramp=end
        displayed=target+(displayed-target)*np.exp(-(dt-fall)/.130);values.append(displayed)
    values=np.array(values)
    return float((np.flatnonzero(values<=-.9*drop)[0]-np.flatnonzero(values<=-.1*drop)[0])*.001)

def measure():
    result={'videos':{},'falls':[],'rises':[],'burst_peaks':[],
        'caution':'Video AAC is silent. Alignments use visible play and meter edges; absolute audio-to-GUI latency is not measured.'}
    calibrations={};traces={}
    for stem in ['T087-in','T087-out']:
        x=np.load(WORK/(stem+'_pixels.npz'))['values'];t=x[:,0];play=x[:,7]>0
        play_start=float(t[play][0]);levels=np.array([-36,-24,-12,-6,-12,-24,-36])
        # Rising steps establish only a relative clock, including unknown GUI scheduling delay.
        observed=[];nominal=[6,9,12,32,35,38]
        for event in nominal:
            before=np.median(x[(t>play_start+event-.5)&(t<play_start+event-.2),1])
            after=np.median(x[(t>play_start+event+.5)&(t<play_start+event+1),1])
            m=(t>play_start+event-.3)&(t<play_start+event+.5)
            observed.append(float(t[m][np.flatnonzero(x[m,1]>=before+.5*(after-before))[0]]))
        scale,offset=np.polyfit(nominal,observed,1)
        result['videos'][stem]=dict(play_start_s=play_start,meter_clock_offset_s=float(offset),clock_scale=float(scale),
            rise_alignment_max_residual_ms=float(1000*max(abs(np.array(observed)-(offset+scale*np.array(nominal))))))
        for channel,col,width,groups in [('main',1,583,[3,29]),('b2',4,234,[3]),('b3',5,234,[29])]:
            xx=[];yy=[]
            for group in groups:
                for n,db in enumerate(levels):
                    start=offset+scale*(group+n*3)
                    yy.append(float(np.median(x[(t>start+1.3)&(t<start+2.6),col])/width));xx.append(db)
            f=lambda db,a,k,b:a*10**(db/k)+b
            pars,_=curve_fit(f,xx,yy,p0=[1.015,40.24,-.015],bounds=([.1,10,-.1],[2,80,.1]))
            a,k,b=pars;db=k*np.log10(np.maximum((x[:,col]/width-b)/a,1e-8))
            calibrations[stem+'_'+channel]=dict(a=float(a),k=float(k),b=float(b),rms_error_pixels=float(np.sqrt(np.mean((f(np.array(xx),*pars)-yy)**2))*width))
            traces[stem+'_'+channel]=(t-offset,db)
            for group in groups:
                for delta,event in [(6,group+12),(12,group+15),(12,group+18)]:
                    edge=offset+scale*event
                    high=float(np.median(db[(t>edge-.6)&(t<edge-.2)]));low=float(np.median(db[(t>edge+1.5)&(t<edge+2.5)]))
                    m=(t>=edge-.1)&(t<edge+2.5);tt=t[m];v=db[m]
                    crossings=[float(tt[np.flatnonzero(v<=high-fraction*(high-low))[0]]) for fraction in [.1,.9]]
                    result['falls'].append(dict(video=stem,channel=channel,event_s=event,nominal_drop_db=delta,
                        t10_s=crossings[0]-edge,t90_s=crossings[1]-edge,interval_10_90_s=crossings[1]-crossings[0],
                        ponte_model_interval_s=model_interval(delta)))
                for n in [1,2,3]:
                    edge=offset+scale*(group+n*3)
                    low=float(np.median(db[(t>edge-.6)&(t<edge-.2)]));high=float(np.median(db[(t>edge+.6)&(t<edge+1)]))
                    m=(t>=edge-.1)&(t<edge+.5);tt=t[m];v=db[m]
                    crossings=[float(tt[np.flatnonzero(v>=low+fraction*(high-low))[0]]) for fraction in [.1,.9]]
                    result['rises'].append(dict(video=stem,channel=channel,event_s=group+n*3,interval_10_90_s=crossings[1]-crossings[0]))
    result['calibrations']=calibrations
    events=next(r for r in json.loads((ROOT/'manifest.json').read_text())['files'] if r['file'].startswith('17_'))['events']
    for suffix in ['in','out']:
        stem='T088-'+suffix;x=np.load(WORK/(stem+'_pixels.npz'))['values'];t=x[:,0]
        start=float(t[x[:,7]>0][0]);result['videos'][stem]=dict(play_start_s=start)
        for channel,col,width in [('main',1,583),('b2',4,234),('b3',5,234)]:
            c=calibrations['T087-'+suffix+'_'+channel]
            db=c['k']*np.log10(np.maximum((x[:,col]/width-c['b'])/c['a'],1e-8))
            traces[stem+'_'+channel]=(t-start,db)
            for e in events:
                if e['label']!='meter_burst':continue
                if channel=='b2' and e['carrier_hz']!=315:continue
                if channel=='b3' and e['carrier_hz']!=2000:continue
                onset=e['start_s'];end=e['end_s'];m=(t>start+onset-.05)&(t<start+end+.15)
                result['burst_peaks'].append(dict(video=stem,channel=channel,onset_s=onset,duration_s=end-onset,
                    nominal_frequency=e['carrier_hz'],visual_peak_db=float(np.max(db[m]))))
    save('video_measurements.json',result)
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(2,2,figsize=(13,8),layout='constrained')
    for ax,stem in zip(axes.flat,['T087-in','T087-out','T088-in','T088-out']):
        for channel in ['main','b2','b3']:
            t,y=traces[stem+'_'+channel];ax.plot(t,y,label=channel,lw=.8)
        ax.set(title=stem,xlabel='relative seconds',ylabel='calibrated displayed dB',ylim=(-65,1),xlim=(0,56));ax.grid(alpha=.2);ax.legend()
    fig.savefig(OUT/'meter_traces.png',dpi=130);plt.close(fig)
    print('Falls by drop:')
    for drop in [6,12]:
        a=[r['interval_10_90_s'] for r in result['falls'] if r['nominal_drop_db']==drop]
        print(drop,'dB:',min(a),np.median(a),max(a),'Ponte model',model_interval(drop))
    print('Maximum rise 10-90:',max(r['interval_10_90_s'] for r in result['rises']))

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--extract',action='store_true');ap.add_argument('--measure',action='store_true');args=ap.parse_args()
    if args.extract:extract()
    if args.measure:measure()
