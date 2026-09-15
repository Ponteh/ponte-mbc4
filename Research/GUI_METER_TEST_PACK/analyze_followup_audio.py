"""04 isolated-band GR and 05 mixed-output comparison, using actual WAV time."""
from pathlib import Path
import sys,json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
sys.path.insert(0,str(Path(__file__).resolve().parent))
from analyze_followup import ROOT,OUT
from analyze_audio import read,envelope,db,high_events

result={'04':{},'05':{}}
stem='04_GR_BAND3_2000Hz'
sr,src,_=read(ROOT/'audio'/(stem+'.wav')); original=high_events(src,sr)
fig,axes=plt.subplots(3,1,figsize=(12,10),layout='constrained')
for plugin,col in [('ORIG','#169b65'),('PONTE','#cd5437')]:
    sr,n,_=read(ROOT/'audio'/(stem+'-'+plugin+'-ratio11.wav'))
    _,c,_=read(ROOT/'audio'/(stem+'-'+plugin+'-ratio21.wav'))
    events=high_events(n,sr); comp_events=high_events(c,sr)
    t,ne=envelope(n,sr);tc,ce=envelope(c,sr);assert np.array_equal(t,tc)
    gr=db(ne)-db(ce);gr[ne<10**(-85/20)]=np.nan
    scale,offset=np.polyfit(original[:,0],events[:,0],1)
    rows=[]
    for i,(start,end) in enumerate(events):
        limit=events[i+1,0]-.05 if i+1<len(events) else t[-1]-.05
        initial=float(np.nanmedian(gr[(t>max(start+.025,end-.2))&(t<end-.02)]))
        row={'onset':start,'release_start':end,'initial_gr_db':initial}
        for frac in [.9,.5,.1]:
            ids=np.where((t>end+.02)&(t<limit)&(gr<=initial*frac))[0]
            hit=next((i for i in ids if np.all(gr[i:i+5]<=initial*frac)),None)
            row['t'+str(round(frac*100))]=float(t[hit]-end) if hit is not None else None
        rows.append(row)
    result['04'][plugin]={'events':rows,'time_scale':float(scale),'offset':float(offset),
        'max_timing_fit_residual_ms':float(np.max(np.abs(events[:,0]-(scale*original[:,0]+offset)))*1000),
        'compressed_onsets':comp_events[:,0].tolist(),'neutral_onsets':events[:,0].tolist(),
        'plateau_gr_db':float(np.nanmedian(gr[(t>events[0,0]+.5)&(t<events[0,1]-.2)]))}
    np.savetxt(OUT/('04_audio_gr_'+plugin+'.csv'),np.c_[t,gr,db(ne),db(ce)],delimiter=',',header='time_s,gr_db,neutral_rms_dbfs,compressed_rms_dbfs',comments='')
    axes[0].plot(t,gr,color=col,label=plugin)
    axes[1].plot(t-events[0,1],gr,color=col,label=plugin)

stem='05_FUNDAMENTAL_PROBE_90_120_180Hz'
sr,src,_=read(ROOT/'audio'/(stem+'.wav')); source_events=high_events(src,sr)
traces={};waveforms={}
for plugin,col in [('ORIG','#169b65'),('PONTE','#cd5437')]:
    sr,x,_=read(ROOT/'audio'/(stem+'-'+plugin+'-BAND23.wav'))
    waveforms[plugin]=x[:,0]
    events=high_events(x,sr);t,e=envelope(x,sr);traces[plugin]=(t,e)
    row={'events_seconds':events.tolist(),'count':len(events)}
    if len(events)==len(source_events):
        scale,offset=np.polyfit(source_events[:,0],events[:,0],1)
        row.update(time_scale=float(scale),offset=float(offset),max_fit_residual_ms=float(np.max(np.abs(events[:,0]-(scale*source_events[:,0]+offset)))*1000))
    result['05'][plugin]=row
    axes[2].plot(t,db(e),color=col,label=plugin)
t,o=traces['ORIG'];tp,p=traces['PONTE'];assert np.array_equal(t,tp)
delta=db(p)-db(o)
rows=[]
for i,(start,end) in enumerate(result['05']['ORIG']['events_seconds']):
    valid=(t>start+.08)&(t<end-.025)
    if not np.any(valid):continue
    start_sample=round((start+.08)*sr);end_sample=round((end-.025)*sr)
    integrated=float(db(np.sqrt(np.mean(waveforms['PONTE'][start_sample:end_sample]**2)))-
        db(np.sqrt(np.mean(waveforms['ORIG'][start_sample:end_sample]**2))))
    rows.append({'event':i+1,'f0_hz':[90,120,180][i//4],
        'integrated_rms_ponte_minus_original_db':integrated,
        'median_ponte_minus_original_db':float(np.median(delta[valid])),
        'p05_db':float(np.percentile(delta[valid],5)),'p95_db':float(np.percentile(delta[valid],95))})
result['05']['output_level_comparison']=rows
np.savetxt(OUT/'05_output_comparison.csv',np.c_[t,db(o),db(p),delta],delimiter=',',header='time_s,orig_rms_dbfs,ponte_rms_dbfs,ponte_minus_orig_db',comments='')
axes[0].set(title='04: GR from ratio 1:1 / ratio 2:1 WAVs',ylabel='GR (dB)',xlabel='Actual WAV seconds',ylim=(-.5,13))
axes[1].set(title='04: first release',ylabel='GR (dB)',xlabel='Seconds after actual release',xlim=(-.1,1.6),ylim=(-.5,13))
axes[2].set(title='05: mixed output RMS (not isolated-band GR)',ylabel='RMS dBFS',xlabel='Actual WAV seconds',ylim=(-65,0))
for ax in axes:ax.grid(alpha=.2);ax.legend()
fig.savefig(OUT/'audio_04_05.png',dpi=150)
(OUT/'audio_measurements.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
