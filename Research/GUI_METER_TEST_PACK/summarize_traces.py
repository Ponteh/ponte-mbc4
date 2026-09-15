"""Exploratory calibrated traces and plateau/event diagnostics."""
from pathlib import Path
import json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
OUT = Path(__file__).resolve().parent/'analysis_2026-09-15'

def read(path): return np.genfromtxt(path,delimiter=',',names=True)
def calibrate(d, original, channel):
    if original:
        # Original scale is nonlinear: ticks -24/-12/-6 at ~25/50/71%.
        # This candidate calibration is checked against A1 known plateaus.
        if channel=='b2_gr':
            fraction = np.where(d[channel+'_count']>0,d[channel+'_left']/233,1.)
            return -40*np.log10(np.maximum(fraction,.001))
        width = 583 if channel.startswith('main') else 233
        return 40*np.log10(np.maximum(d[channel+'_right']/width,.001))
    width = 230 if channel.startswith('main') else 347
    fraction = d[channel+'_right']/width
    return fraction*48 if channel=='b2_gr' else fraction*48-48

fig,axes = plt.subplots(4,2,figsize=(15,13),layout='constrained')
summary = {}
for p in sorted(OUT.glob('*_pixels.csv')):
    d=read(p); orig='ORIG' in p.stem
    panel = 0 if p.stem.startswith('01') else 1 if p.stem.startswith('02') else 2 if 'B0' in p.stem else 3
    ax = axes[panel,0 if orig else 1]
    play=d['play_count']>=3
    starts=np.where(np.diff(np.r_[False,play].astype(int))==1)[0]
    ends=np.where(np.diff(np.r_[play,False].astype(int))==-1)[0]
    summary[p.stem]={'play_intervals':[[float(d['time_s'][i]),float(d['time_s'][j])] for i,j in zip(starts,ends)]}
    for chan in ['main','b2_io','b3_io','b2_gr']:
        y=calibrate(d,orig,chan)
        ax.plot(d['time_s'],y,label=chan,lw=.8)
    ax.set(title=p.stem[:20],ylim=(-60,15),xlabel='Video PTS (s)',ylabel='dB (candidate calibration)')
    ax.grid(alpha=.2);ax.legend(fontsize=8)
fig.savefig(OUT/'video_overview.png',dpi=130)
print(json.dumps(summary,indent=2))
(OUT/'play_intervals.json').write_text(json.dumps(summary,indent=2))
