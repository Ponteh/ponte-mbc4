"""Read-only analysis of submitted WAVs. Run from the PonteDSP workspace root."""
from pathlib import Path
import hashlib
import json
import numpy as np
from scipy.io import wavfile
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

PACK = Path(__file__).resolve().parent
AUDIO = PACK / 'audio'
OUT = PACK / 'analysis_2026-09-15'
OUT.mkdir(exist_ok=True)

def read(path):
    sr, raw = wavfile.read(path)
    dtype = str(raw.dtype)
    if np.issubdtype(raw.dtype, np.integer):
        raw = raw.astype(np.float64) / (2 ** (raw.dtype.itemsize * 8 - 1))
    else:
        raw = raw.astype(np.float64)
    if raw.ndim == 1:
        raw = raw[:, None]
    return sr, raw, dtype

def envelope(x, sr):
    block = round(sr * .01)
    count = len(x) // block
    rms = np.sqrt(np.mean(x[:count*block, 0].reshape(count, block)**2, axis=1))
    return (np.arange(count)+.5)*block/sr, rms

def db(x):
    return 20*np.log10(np.maximum(x, 1e-12))

def high_events(x, sr):
    # 1 ms RMS, threshold far between the -42 dB baseline and -6 dB bursts.
    block = sr//1000
    env = np.sqrt(np.mean(x[:len(x)//block*block,0].reshape(-1,block)**2,axis=1))
    on = env > 10**(-25/20)
    edges = np.diff(np.r_[False,on,False].astype(int))
    raw = np.c_[np.where(edges==1)[0]/1000,np.where(edges==-1)[0]/1000]
    merged = []
    for start,end in raw:
        if merged and start-merged[-1][1] < .006:
            merged[-1][1] = end
        else: merged.append([start,end])
    return np.array(merged)

def main():
    waves, audit, events = {}, {}, {}
    for path in sorted(AUDIO.glob('*.wav')):
        # Freeze the first batch: later 04/05 exports belong to the follow-up.
        if path.name.startswith(('04_', '05_')) and any(p in path.name for p in ['-ORIG','-PONTE']):
            continue
        sr, x, dtype = read(path)
        t, env = envelope(x, sr)
        waves[path.stem] = (t, env)
        if path.name.startswith(('02_', '03_')):
            events[path.stem] = high_events(x,sr)
        audit[path.name] = dict(sample_rate=sr, frames=len(x), channels=x.shape[1],
            format=dtype, duration=len(x)/sr, peak_dbfs=float(db(np.max(np.abs(x)))),
            lr_max_difference=float(np.max(np.abs(x[:,0]-x[:,-1]))),
            sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    results = {'files': audit, 'alignment_envelope_lags_seconds': {}, 'event_timing': {}, 'gr': {}}
    for stem in ['02_IN_OUT_BURSTS','03_GR_BAND2_315Hz']:
        original = events[stem]
        for key, found in events.items():
            if key==stem or not key.startswith(stem): continue
            row = {'high_events_seconds':found.tolist(), 'count':len(found)}
            if len(found)==len(original):
                scale,offset = np.polyfit(original[:,0],found[:,0],1)
                row.update(time_scale=float(scale),offset_seconds=float(offset),
                    maximum_onset_fit_error_ms=float(np.max(np.abs(found[:,0]-scale*original[:,0]-offset))*1000))
            results['event_timing'][key] = row
    fig, axes = plt.subplots(3, 1, figsize=(12, 10), layout='constrained')
    for panel, stem in enumerate(['01_IN_OUT_LEVELS', '02_IN_OUT_BURSTS']):
        source_t, source = waves[stem]
        axes[panel].plot(source_t, db(source*np.sqrt(2)), label='Source', color='.7', lw=1)
        for plugin, colour in [('ORIG', '#169b65'), ('PONTE', '#cd5437')]:
            t, env = waves[stem+'-'+plugin]
            # Keep real file time: a single correlation lag cannot undo time stretching.
            axes[panel].plot(t, db(env*np.sqrt(2)), label=plugin, color=colour, lw=.8)
        axes[panel].set(title=stem, ylabel='Sine amplitude estimate (dBFS peak)', ylim=(-65, 0))
        axes[panel].legend(); axes[panel].grid(alpha=.2)
    stem = '03_GR_BAND2_315Hz'
    for plugin, colour in [('ORIG', '#169b65'), ('PONTE', '#cd5437')]:
        tn, neutral = waves[stem+'-'+plugin+'-ratio11']
        tc, compressed = waves[stem+'-'+plugin+'-ratio21']
        n = min(len(neutral), len(compressed))
        t = tn[:n]
        gr = db(neutral[:n])-db(compressed[:n])
        gr[neutral[:n] < 10**(-85/20)] = np.nan
        axes[2].plot(t, gr, color=colour, label=plugin, lw=1)
        rows = []
        found = events[stem+'-'+plugin+'-ratio11']
        for event_index, (start, end) in enumerate(found):
            before = (t>=max(start, end-.2)) & (t<end-.02)
            initial = float(np.nanmedian(gr[before]))
            row = dict(event_start=start, release_start=end, initial_gr_db=initial)
            for frac in [.5, .1]:
                limit = found[event_index+1,0] if event_index+1<len(found) else t[-1]
                eligible = np.where((t>=end+.02) & (t<limit-.05) & (gr<=initial*frac))[0]
                # Require five consecutive 10 ms windows to avoid isolated crossings.
                hit = next((i for i in eligible if i+5<=n and np.all(gr[i:i+5]<=initial*frac)), None)
                row['t'+str(round(frac*100))+'_seconds'] = None if hit is None else float(t[hit]-end)
            rows.append(row)
        plateau_mask = (t>found[0,0]+.5)&(t<found[0,1]-.2)
        results['gr'][plugin] = dict(events=rows,
            baseline_db=float(np.nanmedian(gr[(t>2.5)&(t<3.5)])),
            plateau_db=float(np.nanmedian(gr[(t>found[0,0]+.5)&(t<found[0,1]-.2)])),
            neutral_plateau_rms_dbfs=float(np.nanmedian(db(neutral[plateau_mask]))))
        np.savetxt(OUT / ('audio_gr_'+plugin+'.csv'), np.c_[t,gr,db(neutral[:n]),db(compressed[:n])],
            delimiter=',', header='time_s,gr_db,neutral_rms_dbfs,compressed_rms_dbfs', comments='')
    axes[2].set(title='03: audio GR = RMS neutral / RMS compressed (10 ms windows)',
        ylabel='Gain reduction (dB)', xlabel='File time (s)', ylim=(-1, 16))
    axes[2].legend(); axes[2].grid(alpha=.2)
    for a in axes[:2]: a.set_xlabel('Actual file time (s); source and exports may have different event timing')
    fig.savefig(OUT/'audio_overview.png', dpi=160)
    (OUT/'audio_results.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    print(json.dumps(results, indent=2))

if __name__ == '__main__': main()
