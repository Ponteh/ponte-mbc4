"""Validate pack integrity and partial/full render deliveries, never DSP fidelity."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import re
import struct

import numpy as np

ROOT = Path(__file__).resolve().parent


def wav_info(raw):
    if raw[:4] != b'RIFF' or raw[8:12] != b'WAVE':
        raise ValueError('Expected RIFF/WAVE (not RF64, compressed audio or renamed file)')
    declared = struct.unpack_from('<I',raw,4)[0]+8
    if declared != len(raw):
        raise ValueError('RIFF length does not match physical file length')
    pos=12
    fmt=data=None
    while pos+8 <= len(raw):
        kind=raw[pos:pos+4];size=struct.unpack_from('<I',raw,pos+4)[0]
        start=pos+8;end=start+size
        if end>len(raw):raise ValueError('Truncated RIFF chunk')
        if kind==b'fmt ':fmt=raw[start:end]
        if kind==b'data':
            if data is not None:raise ValueError('Multiple data chunks are not supported')
            data=raw[start:end]
        pos=end+(size&1)
    if fmt is None or data is None or len(fmt)<16:raise ValueError('Missing fmt/data')
    code,channels,rate,byte_rate,align,bits=struct.unpack_from('<HHIIHH',fmt)
    if code==0xfffe:
        if len(fmt)<40:raise ValueError('Truncated extensible format')
        valid_bits=struct.unpack_from('<H',fmt,18)[0]
        if valid_bits != bits:raise ValueError('Unsupported padded sample representation')
        guid=fmt[24:40]
        if guid[4:] != bytes.fromhex('00001000800000aa00389b71'):
            raise ValueError('Unknown extensible audio GUID')
        code=struct.unpack_from('<I',guid)[0]
    if not (code==1 and bits in (16,24,32) or code==3 and bits==32):
        raise ValueError(f'Unsupported format code/bits {code}/{bits}')
    if channels<=0 or align != channels*(bits//8) or byte_rate != rate*align:
        raise ValueError('Inconsistent channel/sample alignment')
    if not data or len(data)%align:raise ValueError('Empty or incomplete audio frames')
    if code==3:x=np.frombuffer(data,dtype='<f4').astype(np.float64)
    elif bits==16:x=np.frombuffer(data,dtype='<i2').astype(np.float64)/32768
    elif bits==32:x=np.frombuffer(data,dtype='<i4').astype(np.float64)/2147483648
    else:
        b=np.frombuffer(data,dtype=np.uint8).reshape(-1,3)
        v=b[:,0].astype(np.int32)|(b[:,1].astype(np.int32)<<8)|(b[:,2].astype(np.int32)<<16)
        x=((v^0x800000)-0x800000).astype(np.float64)/8388608
    if not np.isfinite(x).all():raise ValueError('NaN/Inf audio samples')
    peak=float(np.max(np.abs(x)))
    return dict(sample_rate=rate,channels=channels,bits=bits,
                format='float' if code==3 else 'PCM',frames=len(data)//align,
                duration_s=len(data)//align/rate,peak_dbfs=20*math.log10(max(peak,1e-15)),
                all_silent=not np.any(x),samples_at_or_over_full_scale=int(np.count_nonzero(np.abs(x)>=1)))


def self_test():
    def make(code,bits,payload):
        align=2*bits//8
        fmt=struct.pack('<HHIIHH',code,2,48000,48000*align,align,bits)
        body=b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(payload))+payload
        return b'RIFF'+struct.pack('<I',len(body))+body
    good=make(3,32,struct.pack('<ffff',.25,-.25,0,0))
    assert wav_info(good)['frames']==2
    assert wav_info(make(1,24,bytes([0,0,64,0,0,192])))['channels']==2
    for bad in (good[:-1],make(3,32,struct.pack('<ff',float('nan'),0)),b'not audio'):
        try:wav_info(bad)
        except (ValueError,struct.error):pass
        else:raise AssertionError('Malformed file accepted')


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--complete',action='store_true')
    args=ap.parse_args()
    self_test()
    manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
    plan=json.loads((ROOT/'render_plan.json').read_text(encoding='utf-8'))
    metadata_path=ROOT/'session_metadata.json'
    unsupported={}
    if metadata_path.exists():
        unsupported=json.loads(metadata_path.read_text(encoding='utf-8')).get('unsupported_test_ids_and_reason',{})
    result=dict(source_checks=[],render_checks=[],missing=[],unsupported=unsupported,extra=[],errors=[],warnings=[])
    source_map={s['file']:s for s in manifest['files']}
    for f in manifest['files']:
        try:
            raw=(ROOT/'audio'/f['file']).read_bytes()
            if hashlib.sha256(raw).hexdigest()!=f['sha256']:raise ValueError('Source hash mismatch')
            info=wav_info(raw)
            assert info['frames']==60*f['sample_rate'] and info['channels']==2
            assert info['sample_rate']==f['sample_rate'] and info['bits']==24
            result['source_checks'].append(f['file'])
        except Exception as e:result['errors'].append(f['file']+': '+str(e))
    known=set()
    ids=set()
    for r in plan:
        assert r['id'] not in ids
        ids.add(r['id'])
        assert r['source'] in source_map
        assert source_map[r['source']]['sample_rate']==r['sample_rate']
        assert r['expected_frames']==r['sample_rate']*60
        count={'MC202':2,'MC303':3,'MC404':4}[r['model']]
        assert len(r['IN'])==len(r['band_settings'])==count and all(r['IN'])
        assert all(1<=b<=count for b in r['SOLO'])
        if r['sidechain']:assert source_map[r['sidechain']]['sample_rate']==r['sample_rate']
        matches=sorted((ROOT/'renders').glob(r['output'].replace('_take1.wav','_take*.wav')))
        matches=[p for p in matches if re.search(r'_take[1-9][0-9]*\.wav$',p.name)]
        if not matches:
            if r['id'] not in unsupported:result['missing'].append(dict(id=r['id'],output=r['output'],stage=r['stage']))
            elif not str(unsupported[r['id']]).strip():result['errors'].append(r['id']+': unsupported needs a reason')
        for p in matches:
            known.add(p.name)
            try:
                raw=p.read_bytes();info=wav_info(raw)
                if (info['sample_rate'],info['frames'],info['channels']) != (r['sample_rate'],r['expected_frames'],2):
                    raise ValueError('Expected stereo, '+str(r['sample_rate'])+' Hz, '+str(r['expected_frames'])+' frames; got '+str(info))
                if info['bits']<24:raise ValueError('Export below 24-bit resolution')
                if info['all_silent']:raise ValueError('Entire output is silent: check routing/IN/SOLO')
                if info['samples_at_or_over_full_scale']:
                    result['warnings'].append(p.name+': full-scale samples; inspect clipping or legitimate float overshoot')
                result['render_checks'].append(dict(file=p.name,sha256=hashlib.sha256(raw).hexdigest(),**info))
            except Exception as e:result['errors'].append(p.name+': '+str(e))
    for k in unsupported:
        if k not in ids:result['errors'].append('Unknown unsupported test ID '+k)
    for p in (ROOT/'renders').glob('*.wav'):
        if p.name in known:continue
        if p.name.startswith('VOICE_'):
            try:
                raw=p.read_bytes();info=wav_info(raw)
                if (info['frames'],info['sample_rate'],info['channels']) != (2880000,48000,2):
                    raise ValueError('VOICE requires stereo 48kHz, 60s')
                if info['all_silent']:raise ValueError('VOICE is entirely silent')
                result['render_checks'].append(dict(file=p.name,sha256=hashlib.sha256(raw).hexdigest(),**info))
            except Exception as e:result['errors'].append(p.name+': '+str(e))
        else:result['extra'].append(p.name)
    if result['extra']:result['warnings'].append('Unrecognised WAV names: not counted as planned exports')
    failure=bool(result['errors']) or (args.complete and bool(result['missing']))
    result['status']='FAIL' if failure else ('FORMAT CHECKS OK; acquisition pending' if result['missing'] else 'FORMAT CHECKS OK')
    result['scope']='File integrity and plan consistency only; does not verify plugin settings or audio-model fidelity.'
    (ROOT/'validation_results.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(result['status'])
    print('Sources:',len(result['source_checks']),'Valid renders:',len(result['render_checks']),
          'Missing:',len(result['missing']),'Unsupported:',len(unsupported))
    for msg in result['errors']+result['warnings']:print(msg)
    raise SystemExit(1 if failure else 0)


if __name__=='__main__':main()
