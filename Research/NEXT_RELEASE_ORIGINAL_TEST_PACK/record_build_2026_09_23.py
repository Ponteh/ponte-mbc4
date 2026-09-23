"""Record successful final checks and package the local Windows test VST3."""
from pathlib import Path
import ast, hashlib, json, re, xml.etree.ElementTree as ET, zipfile

ROOT=Path(__file__).resolve().parent
PRODUCT=ROOT.parent.parent
WORKSPACE=PRODUCT.parents[1]
BUILD=WORKSPACE/'build/mc2000-corrected-2026-09-23'
OUT=ROOT/'analysis_2026-09-23-corrected'

def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
    return h.hexdigest()

def main():
    suite=ET.parse(BUILD/'ctest.xml').getroot()
    cases=suite.findall('.//testcase')
    assert len(cases)==2 and all(not any(c.find(tag) is not None for tag in ['failure','error','skipped']) for c in cases)
    assert re.search(r'VERSION\s+0\.2\.3', (PRODUCT/'CMakeLists.txt').read_text())
    assert 'dspModelVersion = 8' in (PRODUCT/'Source/DSP/MultiBandCompressor.h').read_text()
    verified=json.loads((OUT/'verification.json').read_text())
    for name,h in verified['source_sha256'].items():assert sha(PRODUCT/name)==h,name
    for name,h in verified['scripts_sha256'].items():assert sha(ROOT/name)==h,name
    for name in ['analyse_2026_09_23.py','fit_auto_bite_2026_09_23.py','record_build_2026_09_23.py']:
        ast.parse((ROOT/name).read_text(encoding='utf-8'),filename=name)
    old_videos=json.loads((ROOT/'analysis_2026-09-20/video_inventory.json').read_text())
    for name,r in old_videos.items():assert sha(ROOT/'renders'/(name+'.mp4'))==r['sha256'],name
    vst=WORKSPACE/'build/MC2000-bite-2026-09-23/PonteMC2000_artefacts/Release/VST3/Ponte MBC4.vst3'
    binary=vst/'Contents/x86_64-win/Ponte MBC4.vst3'
    assert binary.is_file()
    package=BUILD/'Ponte-MBC4-0.2.3-model8-Windows-x64-test.zip'
    with zipfile.ZipFile(package,'w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(vst.rglob('*')):
            if p.is_file():z.write(p,p.relative_to(vst.parent).as_posix())
    with zipfile.ZipFile(package) as z:assert z.testzip() is None
    result=dict(status='PASS',version='0.2.3',dsp_model=8,platform='Windows x64',configuration='Release',
        tests=[dict(name=c.get('name'),seconds=float(c.get('time','0')),status='PASS') for c in cases],
        tests_junit_sha256=sha(BUILD/'ctest.xml'),vst3=str(vst),vst3_binary_sha256=sha(binary),
        package=str(package),package_sha256=sha(package),videos_unchanged=len(old_videos),
        source_verified=True,analysis_scripts_verified=True,
        research_input_sha256={p.name:sha(p) for p in [ROOT/'render_plan.json',ROOT/'manifest.json',ROOT.parent/'OriginalPackRender.cpp',ROOT.parent/'AutoBiteFit.cpp']},
        tests_source_sha256=sha(PRODUCT/'Tests/MC2000Tests.cpp'),recorder_sha256=sha(Path(__file__)),
        notes='Local test build only; no release publication or DAW installation. Test durations are not a CPU benchmark.')
    (OUT/'build_validation.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
