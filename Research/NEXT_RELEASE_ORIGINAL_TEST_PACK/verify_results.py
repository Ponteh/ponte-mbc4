"""Gate the before/after research renders; original audio remains private."""
from pathlib import Path
import argparse
import hashlib
import json

ROOT=Path(__file__).resolve().parent
PRODUCT=ROOT.parent.parent
OUT=ROOT/'analysis_2026-09-19'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(folder,build):
    before={r['id']:r for r in json.loads((OUT/'comparison_before.json').read_text())}
    after={r['id']:r for r in json.loads((OUT/'comparison_after.json').read_text())}
    assert set(before)==set(after) and len(after)==30, 'Incomplete comparison matrix'
    rows=[]
    for key,new in after.items():
        old=before[key]
        bh=sha(folder/(key+'_before.f32'));ah=sha(folder/(key+'_after.f32'))
        changed=new['profile'].startswith('AUTO')
        if changed:
            assert new['reduction_mae_db']<old['reduction_mae_db'], key+' Auto regression'
        else:
            assert ah==bh,key+' unexpected neutral/manual audio change'
        rows.append(dict(id=key,profile=new['profile'],before_sha256=bh,after_sha256=ah,
            bit_identical=ah==bh,mae_before=old['reduction_mae_db'],mae_after=new['reduction_mae_db']))
    assert sum(r['profile'].startswith('AUTO') for r in rows)==13
    paths=[PRODUCT/'CMakeLists.txt',*sorted((PRODUCT/'Source').rglob('*.h')),
           *sorted((PRODUCT/'Source').rglob('*.cpp')),*sorted((PRODUCT/'Tests').glob('*.cpp')),
           PRODUCT/'Research/OriginalPackRender.cpp',ROOT/'analyse.py',ROOT/'validate.py',Path(__file__)]
    result=dict(baseline_commit='2dc55d361bd803537aa4feaf186173887c6f439b',version='0.2.3',dsp_model=6,
        scope='MC404, stereo 48 kHz, 512-frame engine blocks; original settings assumed from render plan',
        status='PASS',auto_improved=13,neutral_manual_bit_identical=17,
        source_sha256={str(p.relative_to(PRODUCT)).replace('\\','/'):sha(p) for p in paths},renders=rows)
    if build:
        log=build/'Testing/Temporary/LastTest.log'
        log_text=log.read_text(encoding='utf-8',errors='replace')
        assert log_text.count('Test Passed.')==2 and 'Test Failed.' not in log_text
        for suite in ['MC2000Tests','MC2000UITests']:assert suite in log_text
        bundle=build/'PonteMC2000_artefacts/Release/VST3/Ponte MBC4.vst3'
        assert '"Version": "0.2.3"' in (bundle/'Contents/Resources/moduleinfo.json').read_text()
        result['local_build']=dict(platform='Windows x64 Release VST3',ctest='2/2 PASS',
            ctest_log_sha256=sha(log),binary_sha256=sha(bundle/'Contents/x86_64-win/Ponte MBC4.vst3'),
            renderer_sha256=sha(build/'Release/MC2000OriginalPackRender.exe'))
    (OUT/'verification.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('PASS: 13/13 Auto MAE improvements; 17/17 neutral/manual outputs bit-identical.')


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('folder',type=Path);ap.add_argument('--build',type=Path)
    args=ap.parse_args();verify(args.folder,args.build)
