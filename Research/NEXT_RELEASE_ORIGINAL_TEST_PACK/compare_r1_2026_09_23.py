"""Render DSP_MODEL_7 in a separate build and compare with frozen model 6.

Run with the local numpy/scipy Python: compare_r1_2026_09_23.py --render
Requires MC2000OriginalPackRender in build/MC2000-r1-2026-09-23/Release.
Never rewrites original WAVs, September 20 reports or baseline outputs.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyse import audio

ROOT = Path(__file__).resolve().parent
PRODUCT = ROOT.parent.parent
WORKSPACE = PRODUCT.parents[1]
BASE = ROOT / 'analysis_2026-09-20'
OUT = ROOT / 'analysis_2026-09-23-r1'
BUILD = WORKSPACE / 'build/MC2000-r1-2026-09-23'
DATA = BUILD / 'renders'
EXE = BUILD / 'Release/MC2000OriginalPackRender.exe'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(name, value):
    OUT.mkdir(exist_ok=True)
    (OUT / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def engine(path):
    return np.fromfile(path, dtype='<f4').astype(float).reshape(-1, 2)


def reduction(x, neutral, rate):
    n = round(rate * .01)
    rms = lambda a: np.sqrt(np.mean(a.reshape(-1, n, 2)**2, axis=(1, 2)))
    a, b = rms(x), rms(neutral)
    return 20*np.log10(np.maximum(b, 1e-15)/np.maximum(a, 1e-15)), b


def gain_ls(x, neutral):
    a, b = x[:, 0].reshape(-1, 48), neutral[:, 0].reshape(-1, 48)
    return -20*np.log10(np.maximum(np.sum(a*b, axis=1)/np.maximum(np.sum(b*b, axis=1), 1e-30), 1e-12))


def main(render):
    selected = read(BASE / 'selection.json')['selected']
    rows = {r['id']: r for r in read(ROOT / 'render_plan.json') if r['id'] in selected}
    baseline = read(WORKSPACE / 'build/mc2000-auto-2026-09-20/outputs.json')
    baseline_hashes = read(BASE / 'engine_output_hashes.json')
    inventory = {r['file']: r for r in read(BASE / 'inventory.json')}
    manifest = {r['file']: r for r in read(ROOT / 'manifest.json')['files']}
    DATA.mkdir(exist_ok=True)
    jobs, sources = [], {}
    for key, r in rows.items():
        assert sha(Path(baseline[key])) == baseline_hashes[key], key + ' changed baseline'
        assert sha(ROOT / 'renders' / (key+'.wav')) == inventory[key+'.wav']['sha256'], key + ' changed original'
        name = r['source']
        if name not in sources:
            assert sha(ROOT / 'audio' / name) == manifest[name]['sha256'], name
            sources[name] = manifest[name]['sha256']
            if render:
                rate, x = audio(ROOT / 'audio' / name)
                assert rate == r['sample_rate']
                x.astype('<f4').tofile(DATA / (name+'.f32'))
        cross = r['crossover_hz'] + [1000,10000][len(r['crossover_hz'])-1:] if len(r['crossover_hz'])<3 else r['crossover_hz']
        assert len(cross)==3 and all(a<b for a,b in zip(cross,cross[1:]))
        fields = [json.dumps((DATA/(name+'.f32')).as_posix()), json.dumps((DATA/(key+'.f32')).as_posix()),
                  r['sample_rate'], len(r['IN']), r['input_db'], r['output_db'], *cross]
        for i in range(4):
            if i < len(r['band_settings']):
                b=r['band_settings'][i]
                fields.extend([int(r['IN'][i]),int(i+1 in r['SOLO']),b['gain_db'],b['threshold_db'],b['ratio'],
                               b['knee'],b['bite'],b['attack_ms'],b['release_ms'],{'R1':0,'R2':1,'AUTO':2}[b['mode']]])
            else:
                fields.extend([1,0,0,0,1,0,1,2.5,250,0])
        jobs.append(' '.join(map(str,fields)))
    if render:
        paths = [DATA / f'jobs_{i}.txt' for i in range(3)]
        for i, path in enumerate(paths):
            path.write_text('\n'.join(jobs[i::3])+'\n', encoding='utf-8')
        def run(path):
            subprocess.run([str(EXE), str(path)], check=True, stdout=subprocess.DEVNULL)
        with ThreadPoolExecutor(max_workers=3) as pool:
            list(pool.map(run, paths))
    comparison, releases, unchanged, output_hashes = [], [], [], {}
    for key, r in rows.items():
        ref=r['neutral_reference_id']
        rate,o=audio(ROOT/'renders'/(key+'.wav'))
        _,neutral=audio(ROOT/'renders'/(ref+'.wav'))
        old,old0=engine(baseline[key]),engine(baseline[ref])
        new,new0=engine(DATA/(key+'.f32')),engine(DATA/(ref+'.f32'))
        assert new.shape==old.shape==o.shape==neutral.shape
        assert np.isfinite(new).all()
        changed = any(b['mode']=='R1' and b['ratio']>1 and r['IN'][i] for i,b in enumerate(r['band_settings']))
        if not changed:
            assert np.array_equal(old,new), key+' non-R1 regression'
            unchanged.append(key)
        go,lo=reduction(o,neutral,rate)
        item=dict(id=key,profile=r['profile'],r1_active=changed)
        for label,x,x0 in [('before',old,old0),('after',new,new0)]:
            g,l=reduction(x,x0,rate)
            mask=(lo>10**(-65/20))&(l>10**(-65/20))
            error=abs(g[mask]-go[mask])
            item[label]=dict(mae_db=float(np.mean(error)),p95_db=float(np.quantile(error,.95)))
        comparison.append(item)
        output_hashes[key]=sha(DATA/(key+'.f32'))
        if key in ['T003','T011','T085','T086']:
            assert rate==48000
            traces={label:gain_ls(x,x0) for label,x,x0 in [('original',o,neutral),('before',old,old0),('after',new,new0)]}
            t=(np.arange(len(traces['original']))+.5)/1000
            for end in [22,33,43]:
                segment=dict(id=key,end_s=end)
                mask=(t>end+.025)&(t<end+3)&(traces['original']>.03)
                for label,g in traces.items():
                    initial=float(np.median(g[(t>end-.1)&(t<end-.02)]))
                    detail=dict(initial_gr_db=initial)
                    for p in [90,50,10]:
                        hits=t[(t>=end)&(t<end+3)&(g<=initial*p/100)]
                        detail[f't{p}_ms']=float((hits[0]-end)*1000) if len(hits) else None
                    if label!='original':
                        detail['rmse_db']=float(np.sqrt(np.mean((g[mask]-traces['original'][mask])**2)))
                    segment[label]=detail
                releases.append(segment)
    save('comparison.json',comparison)
    save('release_segments.json',releases)
    save('verification.json',dict(dsp_model=7,baseline_commit='5d8bd92709b113a7957db591a609ef3044dd350e',
         renderer_sha256=sha(EXE),originals_verified=len(rows),bit_identical_non_r1=unchanged,
         original_sha256={k:inventory[k+'.wav']['sha256'] for k in rows},source_audio_sha256=sources,
         source_sha256={p.relative_to(PRODUCT).as_posix():sha(p) for p in sorted((PRODUCT/'Source').rglob('*')) if p.is_file()},
         output_sha256=output_hashes,script_sha256=sha(Path(__file__))))
    for item in comparison:
        if item['r1_active']:
            print(item['id'],item['before'],item['after'])
    print('Verified:',len(rows),'comparisons;',len(unchanged),'bit-identical non-R1 cases',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--render',action='store_true')
    main(parser.parse_args().render)
