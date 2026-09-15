"""Calibrated new video traces with no presumed audio/recording identity."""
from pathlib import Path
import sys,json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
sys.path.insert(0,str(Path(__file__).resolve().parent))
from analyze_followup import ROOT,OLD,OUT

C=json.loads((OLD/'measurements.json').read_text())['calibration']
A=json.loads((OUT/'audio_measurements.json').read_text())
I=json.loads((OUT/'trace_inventory.json').read_text())
D={p.stem.removesuffix('_pixels'):np.genfromtxt(p,delimiter=',',names=True) for p in OUT.glob('*_pixels.csv')}
def odb(f):return C['k']*np.log10(np.maximum((f-C['b'])/C['a'],1e-8))
def value(key,ch):
    d=D[key];r=next(r for r in I[key+'.mp4']['rois'] if r[0]==ch);w=r[3]
    if 'ORIG' in key:
        if ch.endswith('_gr'):
            return np.where(d[ch+'_count']>0,-odb(np.where(d[ch+'_count']>0,d[ch+'_left']/w,1.)),0.)
        return odb(d[ch+'_right']/w)
    return d[ch+'_right']/w*48-(0 if ch.endswith('_gr') else 48)
def med(t,y,a,b):return float(np.median(y[(t>a)&(t<b)]))
def get(prefix,plugin,mode=''):
    return next(k for k in D if k.startswith(prefix) and plugin in k and mode in k)

R={'videos':{},'04':{},'02':{},'03':{},'05':{}}
fig,axes=plt.subplots(4,2,figsize=(15,13),layout='constrained')
for key,d in D.items():
    play=d['play_count']>=3
    # First run lasting at least one second, not an isolated pointer pixel.
    edges=np.diff(np.r_[False,play,False].astype(int)); starts=np.where(edges==1)[0];ends=np.where(edges==-1)[0]-1
    start=next(float(d['time_s'][s]) for s,e in zip(starts,ends) if d['time_s'][e]-d['time_s'][s]>1)
    end=float(d['time_s'][ends[-1]]);t=d['time_s']-start
    R['videos'][key]={'play_start':start,'play_end':end}
    valid=(t>.2)&(d['time_s']<end-.2)
    for ch in ['b2_gr','b3_gr']:
        R['videos'][key][ch+'_maximum']=float(np.max(value(key,ch)[valid]))
    row=0 if key.startswith('02') else 1 if key.startswith('03') else 2 if key.startswith('04') else 3
    col=0 if 'ORIG' in key or key.startswith('02') else 1
    ax=axes[row,col]
    for ch in ['main','b2_io','b2_gr','b3_io','b3_gr']:
        ax.plot(t,value(key,ch),label=ch+(' B0' if 'B0' in key else ''),lw=.8,alpha=.7 if 'B0' in key else 1)
    ax.set(title=key[:32],xlabel='Seconds from visible play',ylabel='Displayed dB',ylim=(-55,14));ax.grid(alpha=.2);ax.legend(fontsize=7)

key=get('02','PONTE');d=D[key];t=d['time_s']-R['videos'][key]['play_start']
old_events=json.loads((OLD/'audio_results.json').read_text())['event_timing']['02_IN_OUT_BURSTS-PONTE']['high_events_seconds']
y_all=np.maximum(value(key,'b2_io'),value(key,'b3_io'))
observed=np.clip((y_all+48)/42,0,1)
def offset_score(offset):
    q=t-offset
    predicted=np.zeros(len(t))
    for start,end in old_events:predicted[(q>=start)&(q<end)]=1
    return float(np.dot(observed-observed.mean(),predicted-predicted.mean()))
grid=np.arange(-1,5,1/120)
coarse=float(max(grid,key=offset_score))
offsets=[]
for i in [12,13,14,27,28,29]:
    start,end=old_events[i];y=value(key,'b2_io' if i<15 else 'b3_io')
    ids=np.where((t>start+coarse-.2)&(t<start+coarse+.25)&(y>-15))[0]
    if len(ids):offsets.append(float(t[ids[0]]-start))
offset=float(np.median(offsets));rows=[]
for i,(start,end) in enumerate(old_events):
    y=value(key,'b2_io' if i<15 else 'b3_io')
    sel=(t>start+offset-.07)&(t<end+offset+.2)
    peak=float(np.max(y[sel]));rows.append({'index':i+1,'nominal_ms':[10,30,100,300,1000][(i%15)//3],
        'frequency':315 if i<15 else 2000,'peak_db':peak,'frames_over_minus20':int(np.count_nonzero(y[sel]>-20))})
R['02']={'alignment_to_old_export_s':offset,'offset_spread_ms':1000*(max(offsets)-min(offsets)),
    'events':rows,'peaks_below_minus9':sum(r['peak_db'] < -9 for r in rows)}

for plugin in ['ORIG','PONTE']:
    kb=get('04',plugin,'B0');k=get('04',plugin,'B1');d=D[k]
    t=d['time_s']-R['videos'][k]['play_start'];gr=value(k,'b3_gr')
    events=A['04'][plugin]['events'];rows=[]
    for event_index,e in enumerate(events):
        start=e['onset'];end=e['release_start']
        window=np.where((t>start-.05)&(t<end+.10))[0]
        peak_index=window[int(np.argmax(gr[window]))]
        plateau=float(gr[peak_index])
        row={'plateau_gr_db':plateau,'wav_release':end}
        crosses=[]
        for frac in [.9,.5,.1]:
            # Start AFTER the visual peak; short events otherwise include the
            # preceding zero-GR baseline and yield spurious zero-length falls.
            ids=np.where((t>t[peak_index])&(t<end+2)&(gr<=frac*plateau))[0]
            row['t'+str(round(frac*100))+'_transport_relative']=float(t[ids[0]]-end) if len(ids) else None
            crosses.append(row['t'+str(round(frac*100))+'_transport_relative'])
        row['t90_to_t10_s']=crosses[2]-crosses[0] if None not in crosses else None
        rows.append(row)
    R['04'][plugin]={'events':rows}
    for kk,mode in [(kb,'B0'),(k,'B1')]:
        tt=D[kk]['time_s']-R['videos'][kk]['play_start']
        R['04'][plugin][mode+'_main_plateau_db']=med(tt,value(kk,'main'),4.9,6.4)
        R['04'][plugin][mode+'_band_io_plateau_db']=med(tt,value(kk,'b3_io'),4.9,6.4)

for plugin in ['ORIG','PONTE']:
    k=get('05',plugin)
    R['05'][plugin]={ch+'_max':R['videos'][k][ch+'_maximum'] for ch in ['b2_gr','b3_gr']}

k=get('03','PONTE');tt=D[k]['time_s']-R['videos'][k]['play_start']
valid=(tt>2)&(tt<20)
R['03']={'b2_gr_max_db':R['videos'][k]['b2_gr_maximum'],
    'b2_in_out_max_difference_db':float(np.max(np.abs(value(k,'b2_io')[valid]-value(k,'b2_out')[valid]))),
    'b2_neutral_plateau_db':med(tt,value(k,'b2_io'),4.9,6.4)}
fig.savefig(OUT/'video_overview.png',dpi=130)
(OUT/'video_measurements.json').write_text(json.dumps(R,indent=2,allow_nan=False),encoding='utf-8')
print(json.dumps(R,indent=2))

# Compact final figure; raw overview above is a diagnostic only.
fig,ax=plt.subplots(2,2,figsize=(13,9),layout='constrained')
for plugin,color in [('ORIG','#169b65'),('PONTE','#cd5437')]:
    old_m=json.loads((OLD/'measurements.json').read_text())
    old_play=json.loads((OLD/'play_intervals.json').read_text())
    p=next(OLD.glob('02_'+plugin+'*_pixels.csv'));d=np.genfromtxt(p,delimiter=',',names=True)
    t=d['time_s']-old_play[p.stem]['play_intervals'][0][0]-old_m['bursts'][plugin]['video_minus_wav_event_offset_s']
    y=odb(d['b2_io_right']/233) if plugin=='ORIG' else 48*d['b2_io_right']/347-48
    ax[0,0].step(t,y,where='post',label=plugin+' take 1',color=color,ls='--' if plugin=='PONTE' else '-')
k=get('02','PONTE');t=D[k]['time_s']-R['videos'][k]['play_start']-R['02']['alignment_to_old_export_s']
ax[0,0].step(t,value(k,'b2_io'),where='post',label='PONTE take 2, UNLINKED',color='#226fce')
for plugin,color in [('ORIG','#169b65'),('PONTE','#cd5437')]:
    a=np.genfromtxt(OUT/('04_audio_gr_'+plugin+'.csv'),delimiter=',',names=True)
    end=A['04'][plugin]['events'][0]['release_start']
    ax[0,1].plot(a['time_s']-end,a['gr_db'],color=color,label=plugin)
    k=get('04',plugin,'B1');d=D[k]
    plateau=R['04'][plugin]['events'][0]['plateau_gr_db']
    cross=R['04'][plugin]['events'][0]['t90_transport_relative']+end
    t=d['time_s']-R['videos'][k]['play_start']-cross
    ax[1,0].step(t,100*value(k,'b3_gr')/plateau,where='post',color=color,label=plugin)
rows=A['05']['output_level_comparison'];xs=[90,120,180]
groups=[[r['integrated_rms_ponte_minus_original_db'] for r in rows if r['f0_hz']==f] for f in xs]
means=np.array([np.mean(g) for g in groups]);lows=np.array([min(g) for g in groups]);highs=np.array([max(g) for g in groups])
ax[1,1].bar([str(x) for x in xs],means,color='#226fce',width=.55)
ax[1,1].errorbar(range(3),means,yerr=[means-lows,highs-means],fmt='none',color='black',capsize=5)
ax[0,0].set(title='02: first three nominal 10 ms bursts, band 2',xlim=(1.3,4),ylim=(-50,0),xlabel='Old export event time (s), individually aligned',ylabel='Displayed dBFS')
ax[0,1].set(title='04: audio GR, first release',xlim=(-.1,1.3),ylim=(-.3,12),xlabel='Seconds from actual WAV release',ylabel='GR (dB)')
ax[1,0].set(title='04: visual GR fall, aligned at 90%',xlim=(-.1,1.1),ylim=(-2,104),xlabel='Seconds from 90% crossing in each video',ylabel='Percent of plateau GR in dB')
ax[1,1].set(title='05 NEUTRAL: output level difference',ylim=(0,.25),xlabel='Fundamental (Hz)',ylabel='Ponte minus original RMS (dB)')
for a in ax.flat:a.grid(alpha=.2)
for a in [ax[0,0],ax[0,1],ax[1,0]]:a.legend(fontsize=8)
fig.savefig(OUT/'followup_comparison.png',dpi=150)
