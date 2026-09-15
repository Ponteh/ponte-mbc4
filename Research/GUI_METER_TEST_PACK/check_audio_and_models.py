"""Additional independent timing/spectrum checks and held-out meter models."""
from pathlib import Path
import json
import numpy as np
from scipy.io import wavfile
from scipy.optimize import least_squares

ROOT=Path(__file__).resolve().parent;OUT=ROOT/'analysis_2026-09-15'
M=json.loads((OUT/'measurements.json').read_text());A=json.loads((OUT/'audio_results.json').read_text())
PLAY=json.loads((OUT/'play_intervals.json').read_text())
R={'wav_spectrum':{},'models':{},'transition_spans':{}}
for p in sorted((ROOT/'audio').glob('*.wav')):
    if not p.name.startswith(('01_','02_','03_')): continue
    sr,x=wavfile.read(p);x=x[:,0].astype(float)
    if 'ORIG' not in p.name and 'PONTE' not in p.name:x/=2**31
    start,end=(10,11) if p.name.startswith('01') else ((19,19.5) if p.name=='02_IN_OUT_BURSTS.wav' else (14.8,15.2)) if p.name.startswith('02') else ((5.5,6.5) if p.name=='03_GR_BAND2_315Hz.wav' else (4.8,5.8))
    a=x[round(start*sr):round(end*sr)]
    spec=np.abs(np.fft.rfft(a*np.hanning(len(a)),n=sr*4));freq=np.argmax(spec)/4
    R['wav_spectrum'][p.name]={'dominant_frequency_hz':freq,'segment':[start,end],
        'sine_peak_from_rms_dbfs':float(20*np.log10(np.sqrt(2*np.mean(a*a))))}

c=M['calibration']
def odb(frac):return c['k']*np.log10(np.maximum((frac-c['b'])/c['a'],1e-8))
d=np.genfromtxt(OUT/'01_ORIG_A1_IN_48k_neutro_take1_pixels.csv',delimiter=',',names=True)
t=d['time_s']-PLAY['01_ORIG_A1_IN_48k_neutro_take1_pixels']['play_intervals'][0][0]
y=odb(d['main_right']/583)
train=[];test=[]
for row in M['levels']['ORIG']:
    if row['event'] in [19.5,39]:continue # Avoid extrapolating calibration below -36.
    tt=t-row['event']; mask=(tt>-.15)&(tt<1.5)
    (train if row['event']<20 else test).append((tt[mask],y[mask],row['before_db'],row['after_db']))

def predict(name,p,t,b,a):
    u=np.maximum(t-p[0],0)
    if name=='linear_db':return np.maximum(a,b-p[1]*u)
    if name=='exponential_amplitude':
        amp=10**(a/20)+(10**(b/20)-10**(a/20))*np.exp(-u/p[1])
        return 20*np.log10(amp)
    if name=='two_pole_amplitude':
        amp=10**(a/20)+(10**(b/20)-10**(a/20))*(1+u/p[1])*np.exp(-u/p[1])
        return 20*np.log10(amp)
    if name=='smoothed_db_ramp':
        rate,tau=p[1:];duration=(b-a)/rate
        v=np.minimum(u,duration)
        yy=b-rate*(v-tau*(1-np.exp(-v/tau)))
        return a+(yy-a)*np.exp(-np.maximum(u-duration,0)/tau)

for name,start,bounds in [
    ('linear_db',[.1,14],([-.2,1],[.3,100])),
    ('exponential_amplitude',[.1,.2],([-.2,.01],[.3,2])),
    ('two_pole_amplitude',[.05,.15],([-.2,.01],[.3,2])),
    ('smoothed_db_ramp',[.05,16,.1],([-.2,1,.005],[.3,100,1]))]:
    def residual(p,events):return np.concatenate([predict(name,p,t,b,a)-y for t,y,b,a in events])
    fit=least_squares(lambda p:residual(p,train),start,bounds=bounds)
    R['models'][name]={'parameters':fit.x.tolist(),'train_rmse_db':float(np.sqrt(np.mean(residual(fit.x,train)**2))),
        'held_out_2khz_rmse_db':float(np.sqrt(np.mean(residual(fit.x,test)**2)))}
R['models_note']='Fit 315 Hz A1 falls, validate 2 kHz A1 falls. First parameter is nuisance time offset. These are behavioural candidates, not the recovered original algorithm; A2 not a fit target.'

for plugin in ['ORIG','PONTE']:
    p=next(OUT.glob('01_'+plugin+'*_pixels.csv'));d=np.genfromtxt(p,delimiter=',',names=True)
    t=d['time_s']-PLAY[p.stem]['play_intervals'][0][0]
    R['transition_spans'][plugin]={}
    for channel in ['main','b2_io']:
        width=(583 if channel=='main' else 233) if plugin=='ORIG' else (230 if channel=='main' else 347)
        y=odb(d[channel+'_right']/width) if plugin=='ORIG' else 48*d[channel+'_right']/width-48
        rows=[]
        for event in [12,14.5,17]:
            before=np.median(y[(t>event-.5)&(t<event-.1)])
            after=np.median(y[(t>event+1.6)&(t<event+1.9)])
            crossings=[]
            for frac in [.1,.9]:
                threshold=before+(after-before)*frac
                ids=np.where((t>event-.2)&(t<event+2)&(y<threshold))[0]
                crossings.append(float(t[ids[0]]))
            rows.append({'event':event,'drop_db':float(before-after),'10_to_90_percent_drop_s':crossings[1]-crossings[0]})
        R['transition_spans'][plugin][channel]=rows
(OUT/'additional_checks.json').write_text(json.dumps(R,indent=2),encoding='utf-8')
print(json.dumps(R,indent=2))
