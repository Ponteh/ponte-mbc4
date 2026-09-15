"""Calibrate scales, measure transitions and compare GUI GR with WAV ratios.

Calibration uses A1 master plateaus and the corresponding output WAV, then
checks the band meter and A2. Original GR uses the same printed scale reversed.
Absolute audio/video latency is not identifiable (video audio is silent).
"""
from pathlib import Path
import json, re
import numpy as np
from scipy.optimize import least_squares
from scipy.io import wavfile
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'analysis_2026-09-15'
A=json.loads((OUT/'audio_results.json').read_text())
PLAY=json.loads((OUT/'play_intervals.json').read_text())
DATA={p.stem.replace('_pixels',''):np.genfromtxt(p,delimiter=',',names=True) for p in OUT.glob('*_pixels.csv')}
RESULT={'calibration':{},'levels':{},'bursts':{},'gr_visual':{},'frame_audit':{}}

def select(prefix,plugin,mode=''):
    return next((k,v) for k,v in DATA.items() if k.startswith(prefix+'_'+plugin) and mode in k)
def play_start(k): return PLAY[k+'_pixels']['play_intervals'][0][0]
def med(t,y,a,b): return float(np.median(y[(t>=a)&(t<b)]))

# A1: all fourteen stable plateaus, measured near the end of each 2.5 s step.
k,d=select('01','ORIG'); t=d['time_s']-play_start(k)
sr,w=wavfile.read(ROOT/'audio/01_IN_OUT_LEVELS-ORIG.wav')
plateaus=[]
for start in [2+i*2.5 for i in range(7)]+[21.5+i*2.5 for i in range(7)]:
    x=w[int((start+1.3)*sr):int((start+2.2)*sr),0].astype(float)
    level=20*np.log10(np.sqrt(2*np.mean(x*x)))
    fraction=med(t,d['main_right']/583,start+1.3,start+2.2)
    plateaus.append([start,level,fraction])
plateaus=np.array(plateaus)
fit=least_squares(lambda p:p[0]*10**(plateaus[:,1]/p[1])+p[2]-plateaus[:,2],
                  [1,40,0],bounds=([.5,20,-.2],[1.5,80,.2]))
ca,ck,cb=fit.x
def original_db(f): return ck*np.log10(np.maximum((f-cb)/ca,1e-8))
RESULT['calibration']={'original_model':'fraction = a * 10^(dB/k) + b',
    'a':float(ca),'k':float(ck),'b':float(cb),'plateaus_start_audio_db_fraction':plateaus.tolist(),
    'maximum_plateau_error_db':float(np.max(np.abs(original_db(plateaus[:,2])-plateaus[:,1]))),
    'note':'Master fit from A1; same printed nonlinear scale applied to band I/O and reversed GR. Quantization increases near the floor.'}

def level(d,plugin,channel):
    if plugin=='ORIG':
        if channel=='b2_gr':
            f=np.where(d[channel+'_count']>0,d[channel+'_left']/233,1.)
            return np.where(d[channel+'_count']>0,-original_db(f),0.)
        width=583 if channel.startswith('main') else 233
        return original_db(d[channel+'_right']/width)
    width=230 if channel.startswith('main') else 347
    return d[channel+'_right']/width*48-(0 if channel=='b2_gr' else 48)

fig,axes=plt.subplots(2,2,figsize=(13,9),layout='constrained')
for plugin,col in [('ORIG','#169b65'),('PONTE','#cd5437')]:
    k,d=select('01',plugin); t=d['time_s']-play_start(k)
    y=level(d,plugin,'main')
    rows=[]
    for event in [12,14.5,17,19.5,31.5,34,36.5,39]:
        before=med(t,y,event-.5,event-.1)
        after=med(t,y,event+1.5,event+1.9)
        targets=[before-1,before-5] if abs(before-after)<8 else [before-1,before-10]
        crossings=[]
        for target in targets:
            ids=np.where((t>event-.2)&(t<event+2.2)&(y<=target))[0]
            crossings.append(float(t[ids[0]]) if len(ids) else None)
        # Steepest central dB fall, avoiding both the old and new plateaus.
        use=(t>event-.15)&(t<event+2.0)&(y<before-1)&(y>max(after+1,-40))
        slope=float(np.polyfit(t[use],y[use],1)[0]) if np.count_nonzero(use)>=4 else None
        rows.append(dict(event=event,before_db=before,after_db=after,targets_db=targets,
            crossing_times=crossings,central_fall_db_per_s=slope,
            crossing_span_seconds=None if None in crossings else crossings[1]-crossings[0]))
    RESULT['levels'][plugin]=rows
    axes[0,0].step(t-12,y,where='post',label=plugin,color=col)
    axes[0,1].step(t-14.5,y,where='post',label=plugin,color=col)

    k,d=select('02',plugin);t=d['time_s']-play_start(k)
    events=A['event_timing']['02_IN_OUT_BURSTS-'+plugin]['high_events_seconds']
    # Align on the long bursts; first onset includes an unknown fixed host/display lag.
    offsets=[]
    for idx in [12,13,14,27,28,29]:
        start,end=events[idx]; ch='b2_io' if idx<15 else 'b3_io'
        y=level(d,plugin,ch)
        candidates=np.where((t>start-.15)&(t<start+.3)&(y>-15))[0]
        if len(candidates): offsets.append(float(t[candidates[0]]-start))
    offset=float(np.median(offsets)); rows=[]
    for i,(start,end) in enumerate(events):
        ch='b2_io' if i<15 else 'b3_io'; y=level(d,plugin,ch)
        use=(t>=start+offset-.05)&(t<end+offset+.25)
        peak=float(np.max(y[use])); hits=int(np.count_nonzero(y[use]>-20))
        rows.append(dict(index=i+1,frequency=315 if i<15 else 2000,
            nominal_ms=[10,30,100,300,1000][(i%15)//3],wav_onset=start,
            wav_duration_ms=1000*(end-start),visual_peak_db=peak,frames_above_minus20=hits))
    RESULT['bursts'][plugin]={'video_minus_wav_event_offset_s':offset,'events':rows}

    k,d=select('03',plugin,'B1');t=d['time_s']
    gr=level(d,plugin,'b2_gr')
    wav=np.genfromtxt(OUT/('audio_gr_'+plugin+'.csv'),delimiter=',',names=True)
    # Fit ONLY a time offset. No rescaling of GR or time constant to force agreement.
    valid=(t>play_start(k)+3.8)&(t<play_start(k)+18)
    def err(v):
        expected=np.interp(t[valid]-v[0],wav['time_s'],np.nan_to_num(wav['gr_db']))
        return gr[valid]-expected
    shift=least_squares(err,[play_start(k)+.07],diff_step=.001,
        bounds=([play_start(k)-.2],[play_start(k)+.4])).x[0]
    rows=[]
    for event in A['gr'][plugin]['events']:
        start=event['event_start']; end=event['release_start']
        plateau=med(t-shift,gr,max(start+.04,end-.2),end-.03)
        row={'wav_release_s':end,'plateau_db':plateau}
        for frac in [.9,.5,.1]:
            ids=np.where((t-shift>end-.1)&(t-shift<end+2)&(gr<=plateau*frac))[0]
            row['t'+str(round(frac*100))+'_seconds_relative_to_aligned_wav']=float(t[ids[0]]-shift-end) if len(ids) else None
        row['t90_to_t10_seconds']=row['t10_seconds_relative_to_aligned_wav']-row['t90_seconds_relative_to_aligned_wav']
        rows.append(row)
    RESULT['gr_visual'][plugin]={'fitted_video_minus_wav_offset_seconds':float(shift),
        'rmse_db_with_offset_only':float(np.sqrt(np.mean(err([shift])**2))), 'events':rows,
        'caveat':'Offset includes host/display/recorder; WAV PRINT identity and Ponte B0 solo unverified.'}
    end=A['gr'][plugin]['events'][0]['release_start']
    axes[1,0].step(t-shift-end,gr,where='post',color=col,label=plugin+' visual')
    axes[1,0].plot(wav['time_s']-end,wav['gr_db'],color=col,ls='--',label=plugin+' WAV ratio',lw=1)
    k,d=select('02',plugin);t=d['time_s']-play_start(k)-RESULT['bursts'][plugin]['video_minus_wav_event_offset_s']
    axes[1,1].step(t,level(d,plugin,'b2_io'),where='post',color=col,label=plugin)

for ax in axes.flat: ax.grid(alpha=.25);ax.legend(fontsize=8)
axes[0,0].set(title='A1 MAIN: step -6 to -12 dBFS',xlim=(-.2,.9),ylim=(-15,-4),xlabel='Seconds from WAV step (transport aligned)',ylabel='Displayed dBFS')
axes[0,1].set(title='A1 MAIN: step -12 to -24 dBFS',xlim=(-.2,1.2),ylim=(-27,-10),xlabel='Seconds from WAV step (transport aligned)',ylabel='Displayed dBFS')
axes[1,0].set(title='B1: long release, visual and WAV ratio',xlim=(-.15,1.6),ylim=(-.5,12),xlabel='Seconds from actual WAV release (fitted offset)',ylabel='GR (dB)')
axes[1,1].set(title='A2: first three nominal 10 ms bursts, band 2',xlim=(1.3,4.2),ylim=(-55,0),xlabel='Actual WAV seconds (aligned on long bursts)',ylabel='Displayed dBFS')
fig.savefig(OUT/'meter_comparison.png',dpi=160)
for k,d in DATA.items():
    dt=np.diff(d['time_s'])
    RESULT['frame_audit'][k]=dict(frames=len(d),gaps_over_25_ms=np.where(dt>.025)[0].tolist(),
        gap_start_pts=d['time_s'][:-1][dt>.025].tolist(),maximum_interval_seconds=float(np.max(dt)))
(OUT/'measurements.json').write_text(json.dumps(RESULT,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in RESULT.items() if k not in ['bursts','frame_audit']},indent=2))
