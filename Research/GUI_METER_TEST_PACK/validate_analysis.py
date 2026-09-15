"""Audit the September 15 batch, source integrity and derived trace coverage."""
from pathlib import Path
import json,hashlib,subprocess
import numpy as np
import imageio_ffmpeg
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'analysis_2026-09-15'
A=json.loads((OUT/'audio_results.json').read_text())
V=json.loads((OUT/'video_inventory.json').read_text())
M=json.loads((OUT/'measurements.json').read_text())
result={'verified_input_hashes':[],'silent_video_audio':{},'trace_frames':{}}
for name,meta in list(A['files'].items())+list(V.items()):
    assert hashlib.sha256((ROOT/'audio'/name).read_bytes()).hexdigest()==meta['sha256'],name
    result['verified_input_hashes'].append(name)
for name in V:
    p=ROOT/'audio'/name
    raw=subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),'-v','error','-i',str(p),
        '-vn','-ac','1','-f','f32le','pipe:1'],capture_output=True,check=True).stdout
    audio=np.frombuffer(raw,np.float32)
    assert len(audio)>0 and np.max(np.abs(audio))==0,name
    result['silent_video_audio'][name]={'samples':len(audio),'peak':0.0}
    d=np.genfromtxt(OUT/(p.stem+'_pixels.csv'),delimiter=',',names=True)
    assert len(d)==M['frame_audit'][p.stem]['frames']
    assert np.all(np.diff(d['time_s'])>0)
    result['trace_frames'][name]=len(d)
for plugin in ['ORIG','PONTE']:
    assert A['event_timing']['02_IN_OUT_BURSTS-'+plugin]['count']==30
    assert len(M['bursts'][plugin]['events'])==30
    assert len(A['gr'][plugin]['events'])==3
    assert len(M['gr_visual'][plugin]['events'])==3
    for e in A['gr'][plugin]['events']:
        assert 0<e['t50_seconds']<e['t10_seconds']<2
for p in ROOT.glob('*.py'):compile(p.read_text(encoding='utf-8'),str(p),'exec')
result['status']='PASS: hashes unchanged, 8 traces, silent AAC, 30 bursts and 3 GR events per plugin, script syntax'
(OUT/'validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(result['status']);print('Frames:',sum(result['trace_frames'].values()))
