"""Voice cross-check with explicit alignment diagnostics (MP3 is not a B0 export)."""
from pathlib import Path
import sys,json,subprocess,hashlib
import numpy as np
from scipy.signal import correlate
ROOT=Path(__file__).resolve().parent/'NEXT_RELEASE_ORIGINAL_TEST_PACK';sys.path.insert(0,str(ROOT))
from analyse import audio
BUILD=ROOT.parents[3]/'build/mc2000-nap-2026-09-23/voice'
OUT=ROOT/'analysis_2026-09-24'
x=np.fromfile(BUILD/'dry.f32',dtype='<f4').reshape(-1,2)
original_length=len(x)
x=np.pad(x,((96000,0),(0,0))) # allow a delayed MP3 decoder/import start
paths=sorted((ROOT/'renders').glob('T0voc*.wav'))
originals={250:audio(next(p for p in paths if '250rel' in p.name))[1],500:audio(next(p for p in paths if '500rel' in p.name))[1]}
width=480
energy=lambda v:np.sqrt(np.mean(v[:len(v)//width*width].reshape(-1,width,2)**2,axis=(1,2)))
a=energy(x);b=energy(originals[250]);n=len(b)
bc=b-b.mean();dots=correlate(a,bc,mode='valid',method='fft')
sums=np.convolve(a,np.ones(n),mode='valid');squares=np.convolve(a*a,np.ones(n),mode='valid')
norm=np.sqrt(np.maximum(squares-sums*sums/n,1e-30)*np.sum(bc*bc))
correlations=dots/norm;offset=int(np.argmax(correlations));start=offset*width
result=dict(source_sha256=hashlib.sha256((ROOT/'audio/GAFITA Y CHOMBA_Vocal.mp3').read_bytes()).hexdigest(), original_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},mp3_source='GAFITA Y CHOMBA_Vocal.mp3',source_duration_s=original_length/48000,source_offset_s=(start-96000)/48000,envelope_correlation=float(correlations[offset]),native_source_rate=44100,comparison_rate=48000,video_confirmed=dict(crossover_hz=[100,785,10000],mode='R1',all_IN=True,solo=False),limitations=['No original neutral voice export; MP3 decoder/resampler and host clip gain are additional uncertainties.'])
y=x[start:start+len(originals[250])];assert len(y)==len(originals[250]);y.astype('<f4').tofile(BUILD/'aligned_input.f32')
drift=[]
for second in range(5,55,10):
 begin=second*100;goal=b[begin:begin+500];reference=a[offset+begin-50:offset+begin+550]
 c=correlate(reference-reference.mean(),goal-goal.mean(),mode='valid',method='fft');drift.append(dict(at_s=second,local_shift_s=(int(np.argmax(c))-50)*.01))
result['envelope_drift_checks']=drift
(OUT/'voice_alignment.json').write_text(json.dumps(result,indent=2)+'\n')
print(result,flush=True)
if '--render' not in sys.argv and '--measure-only' not in sys.argv:sys.exit(0)
assert result['envelope_correlation']>.8 and max(abs(r['local_shift_s']) for r in drift)<=.03,'alignment not sufficient for DSP comparison'
jobs=[]
for release in [0,250,500]:
 fields=[json.dumps((BUILD/'aligned_input.f32').as_posix()),json.dumps((BUILD/f'ponte_{release}.f32').as_posix()),48000,4,0,0,100,785,10000]
 for band in range(4):fields += [1,0,0,-27.5,2 if release else 1,0,1,2.5,release or 250,0]
 jobs.append(' '.join(map(str,fields)))
(BUILD/'jobs.txt').write_text('\n'.join(jobs)+'\n')
exe=ROOT.parents[3]/'build/MC2000-bite-2026-09-23/Release/MC2000OriginalPackRender.exe'
if '--measure-only' not in sys.argv:subprocess.run([str(exe),str(BUILD/'jobs.txt')],check=True,stdout=subprocess.DEVNULL)
candidate=np.fromfile(BUILD/'ponte_250.f32',dtype='<f4').reshape(-1,2)
lags=[]
for second in [5,15,25,35,45]:
 a0=originals[250][second*48000:(second+1)*48000,0];b0=candidate[second*48000:(second+1)*48000,0]
 c=correlate(a0,b0,mode='full',method='fft');middle=len(b0)-1
 lag=int(np.argmax(c[middle-480:middle+481]))-480
 lags.append(dict(at_s=second,lag_samples=lag,correlation=float(c[middle+lag]/np.sqrt(np.sum(a0*a0)*np.sum(b0*b0)))))
lag=int(np.median([v['lag_samples'] for v in lags]))
result['audio_refinement']=lags
result['additional_output_alignment_samples']=lag
assert max(abs(v['lag_samples']-lag) for v in lags)<=4,'audio timing drift; cannot align with a constant shift'
def align(v):
 if lag>=0:return np.pad(v,((lag,0),(0,0)))[:len(v)]
 return np.pad(v[-lag:],((0,-lag),(0,0)))
neutral=align(np.fromfile(BUILD/'ponte_0.f32',dtype='<f4').reshape(-1,2))
rows=[];curves={}
for release,o in originals.items():
 p=align(np.fromfile(BUILD/f'ponte_{release}.f32',dtype='<f4').reshape(-1,2))
 gr_o=20*np.log10(np.maximum(energy(neutral),1e-15)/np.maximum(energy(o),1e-15))
 gr_p=20*np.log10(np.maximum(energy(neutral),1e-15)/np.maximum(energy(p),1e-15))
 valid=energy(neutral)>10**(-60/20);valid[:100]=False
 difference=gr_p-gr_o
 rows.append(dict(release_ms=release,mae_db=float(abs(difference[valid]).mean()),signed_mean_ponte_minus_original_gr_db=float(difference[valid].mean()),p95_db=float(np.quantile(abs(difference[valid]),.95))))
 curves[release]=(gr_o,gr_p)
result['comparison']=rows
# Release contrast cancels the inferred neutral, but still requires matching takes.
mask=(energy(originals[250])>10**(-65/20))&(energy(originals[500])>10**(-65/20));mask[:100]=False
contrast_o=curves[500][0]-curves[250][0];contrast_p=curves[500][1]-curves[250][1]
result['release_contrast_mae_db']=float(abs(contrast_o[mask]-contrast_p[mask]).mean())
(OUT/'voice_comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print(rows,flush=True)
