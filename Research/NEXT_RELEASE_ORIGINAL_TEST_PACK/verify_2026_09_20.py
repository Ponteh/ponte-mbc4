"""Verify the frozen analysis inputs and provenance, using the standard library."""
from pathlib import Path
import ast, hashlib, json

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'analysis_2026-09-20'
PRODUCT=ROOT.parent.parent
WORKSPACE=PRODUCT.parents[1]
BUILD=WORKSPACE/'build/mc2000-auto-2026-09-20'

def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
    return h.hexdigest()

def read(path):
    def invalid(value):raise ValueError('Non-finite JSON: '+value)
    return json.loads(path.read_text(encoding='utf-8'),parse_constant=invalid)

def main():
    inv=read(OUT/'inventory.json')
    for r in inv:assert sha(ROOT/'renders'/r['file'])==r['sha256'],r['file']
    videos=read(OUT/'video_inventory.json')
    for stem,r in videos.items():assert sha(ROOT/'renders'/(stem+'.mp4'))==r['sha256'],stem
    manifest=read(ROOT/'manifest.json')
    for r in manifest['files']:assert sha(ROOT/'audio'/r['file'])==r['sha256'],r['file']
    provenance=read(OUT/'engine_provenance.json')
    for name,h in provenance['source_sha256'].items():assert sha(PRODUCT/name)==h,name
    assert sha(WORKSPACE/'build/MC2000-analysis-vs/Release/MC2000OriginalPackRender.exe')==provenance['renderer_sha256']
    outputs=read(BUILD/'outputs.json'); hashes=read(OUT/'engine_output_hashes.json')
    for key,h in hashes.items():assert sha(Path(outputs[key]))==h,key
    selection=read(OUT/'selection.json');comparison=read(OUT/'comparison.json')
    assert set(selection['selected'])==set(hashes)=={r['id'] for r in comparison}
    assert len(comparison)==61 and len(selection['missing'])==13 and len(selection['excluded'])==14
    silent=[r['file'] for r in inv if len(r['file'])==8 and r['all_silent']]
    assert len(silent)==12
    scripts=['analyse_2026_09_20.py','diagnose_2026_09_20.py','measure_meters_2026_09_20.py','verify_2026_09_20.py']
    for name in scripts:ast.parse((ROOT/name).read_text(encoding='utf-8'),filename=name)
    for path in OUT.glob('*.json'):read(path)
    data=dict(status='PASS',date='2026-09-20',original_wavs_unchanged=len(inv),original_videos_unchanged=len(videos),
        source_wavs_verified=len(manifest['files']),production_sources_unchanged=len(provenance['source_sha256']),
        renderer_unchanged=True,engine_outputs_verified=len(hashes),silent_primary_files=silent,
        scripts_sha256={name:sha(ROOT/name) for name in scripts},
        scope='Analysis integrity only; not a pass of all acquisition settings, DSP tests or the full release matrix.')
    (OUT/'verification.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(data,indent=2))

if __name__=='__main__':main()
