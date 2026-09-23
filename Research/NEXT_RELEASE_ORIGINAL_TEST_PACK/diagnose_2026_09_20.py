"""Reproduce transient and routing diagnostics; never modify original audio or DSP."""
from pathlib import Path
import json, sys
import numpy as np
from scipy.optimize import curve_fit
sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyse_2026_09_20 import OUT, BUILD, PLAN, get_original, save

OUTPUTS = json.loads((BUILD/'outputs.json').read_text())
ROWS = {r['id']: r for r in PLAN}

def engine(key):
    return np.fromfile(OUTPUTS[key], dtype='<f4').astype(float).reshape(-1, 2)

def gain_ls(a, b, rate):
    # Signed least-squares scalar on an isolated, phase-aligned carrier.
    assert rate == 48000
    a, b = a[:, 0].reshape(-1, 48), b[:, 0].reshape(-1, 48)
    slope = np.sum(a*b, axis=1)/np.maximum(np.sum(b*b, axis=1), 1e-30)
    return -20*np.log10(np.maximum(slope, 1e-12))

def energy(a):
    return np.mean(a[:, 0].reshape(-1, 48)**2, axis=1)

def transients():
    bite, release, fits, plots = [], [], [], {}
    for base, alt in [('T002','T027'),('T002','T028'),('T010','T029'),('T010','T030')]:
        rate, a = get_original(base)
        _, b = get_original(alt)
        _, neutral = get_original(ROWS[base]['neutral_reference_id'])
        assert rate == 48000
        relief = 10*np.log10(np.maximum(energy(b),1e-30)/np.maximum(energy(a),1e-30))
        model = 10*np.log10(np.maximum(energy(engine(alt)),1e-30)/np.maximum(energy(engine(base)),1e-30))
        t = (np.arange(len(relief))+.5)/1000
        for start in [6,12,19]:
            m = (t>=start)&(t<start+.05)&(energy(neutral)>10**(-45/10))
            bite.append(dict(base=base,variant=alt,onset_s=start,
                original_peak_relief_db=float(np.max(relief[m])),
                ponte_peak_relief_db=float(np.max(model[m])),
                max_relief_error_db=float(np.max(abs(relief[m]-model[m])))))
        plots[alt] = (t, relief, model)
    for key in ['T003','T011','T085','T086']:
        ref = ROWS[key]['neutral_reference_id']
        rate,a = get_original(key); _,b = get_original(ref)
        go = gain_ls(a,b,rate); gp = gain_ls(engine(key),engine(ref),rate)
        t = (np.arange(len(go))+.5)/1000
        rel = ROWS[key]['band_settings'][0]['release_ms']
        item = dict(id=key,release_ms=rel)
        for name,g in [('original',go),('ponte',gp)]:
            initial = float(np.median(g[(t>21.9)&(t<21.98)]))
            detail = dict(initial_gr_db=initial)
            for p in [90,50,10]:
                hits = t[(t>=22)&(t<25)&(g<=initial*p/100)]
                detail[f't{p}_ms'] = int(round((hits[0]-22)*1000)) if len(hits) else None
            item[name] = detail
        release.append(item)
        for end in [22,33,43]:
            m = (t>end+.025)&(t<end+3)&(go>.03)
            f = lambda dt,A,tau: 10*np.log10(1+A*np.exp(-dt/tau))
            pars,_ = curve_fit(f,t[m]-end,go[m],p0=[10,rel/1000],bounds=([0,.001],[1000,10]))
            fits.append(dict(id=key,end_s=end,release_ms=rel,A=float(pars[0]),tau_ms=float(pars[1]*1000),
                rmse_db=float(np.sqrt(np.mean((f(t[m]-end,*pars)-go[m])**2)))))
        plots[key] = (t,go,gp)
    save('transient_details.json',dict(bite=bite,release=release))
    save('r1_release_fit.json',fits)
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(2,2,figsize=(12,8),layout='constrained')
    for ax,key in zip(axes.flat,['T027','T030','T085','T086']):
        t,o,p=plots[key]; is_bite=key in ['T027','T030']; origin=6 if is_bite else 22
        m=(t>=origin)&(t<origin+(.06 if is_bite else 2.5))
        ax.plot((t[m]-origin)*1000,o[m],label='McDSP original')
        ax.plot((t[m]-origin)*1000,p[m],label='Ponte DSP_MODEL_6',ls='--')
        if not is_bite:
            fit=next(r for r in fits if r['id']==key and r['end_s']==22)
            ax.plot((t[m]-origin)*1000,10*np.log10(1+fit['A']*np.exp(-(t[m]-origin)/(fit['tau_ms']/1000))),
                label='Linear-control fit',ls=':',color='black')
        ax.set(title=key+(' / BITE relief relative to BITE 1' if is_bite else ' / R1 release 500 ms'),
            xlabel='ms from transition',ylabel='relief dB' if is_bite else 'GR dB')
        ax.grid(alpha=.2);ax.legend(fontsize=8)
    fig.savefig(OUT/'transients.png',dpi=140);plt.close(fig)

def tone_levels(x,rate):
    # Integer-second plateau: four integer-Hz bins, after settling.
    y=x[9*rate:10*rate,0]; spectrum=np.fft.rfft(y)
    return 20*np.log10(np.maximum(2*abs(spectrum[[50,315,2000,14000]])/len(y),1e-30))

def routing():
    rate_rows=[]; tone_rows=[]
    for first in [62,66,70]:
        ids=[f'T{i:03}' for i in range(first,first+4)]
        waves=[get_original(k) for k in ids]; rate=waves[0][0]
        assert all(r==rate for r,_ in waves)
        a,b,c,d=[x for _,x in waves]; delta=a-b; residual=delta-(c-d)
        levels=np.array([tone_levels(x,rate) for x in [a,b,c,d]])
        rate_rows.append(dict(ids=ids,rate=rate,plateau_window_s=[9,10],
            all_minus_low_compression_residual_peak=float(np.max(abs(residual))),
            all_minus_low_compression_relative_db=float(10*np.log10(np.sum(residual**2)/np.sum(delta**2))),
            tones_hz=[50,315,2000,14000],level_db=levels.tolist(),all_tone_reduction_db=(levels[0]-levels[1]).tolist()))
    for first in [40,44,62,66,70]:
        rate,a=get_original(f'T{first:03}'); rb,b=get_original(f'T{first+1:03}');assert rb==rate
        la,lb=tone_levels(a,rate),tone_levels(b,rate)
        tone_rows.append(dict(ids=[f'T{first:03}',f'T{first+1:03}'],rate=rate,plateau_window_s=[9,10],
            tones_hz=[50,315,2000,14000],neutral_db=la.tolist(),compressed_db=lb.tolist(),reduction_db=(la-lb).tolist()))
    save('sample_rate_controls.json',rate_rows);save('steady_tone_controls.json',tone_rows)

if __name__=='__main__':
    transients();routing()
    print('Transient fits, high-resolution figure and routing controls complete.')
