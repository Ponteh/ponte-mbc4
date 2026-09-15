"""Integrity and coverage audit of the follow-up; does not rewrite inputs."""
from pathlib import Path
import sys,json,hashlib
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
from analyze_followup import ROOT,OUT,OLD
from analyze_audio import read

inv=json.loads((OUT/'inventory.json').read_text())
old_video=json.loads((OLD/'video_inventory.json').read_text())
trace=json.loads((OUT/'trace_inventory.json').read_text())
audio=json.loads((OUT/'audio_measurements.json').read_text())
visual=json.loads((OUT/'video_measurements.json').read_text())
result={'unchanged_input_hashes':[],'new_video_frames':0,'carrier_frequency_checks':{}}
for name,meta in list(inv['wav'].items())+list(inv['video'].items())+list(old_video.items()):
    assert hashlib.sha256((ROOT/'audio'/name).read_bytes()).hexdigest()==meta['sha256'],name
    result['unchanged_input_hashes'].append(name)
for name,meta in trace.items():
    d=np.genfromtxt(OUT/(Path(name).stem+'_pixels.csv'),delimiter=',',names=True)
    assert len(d)==meta['frames'] and np.all(np.diff(d['time_s'])>0)
    result['new_video_frames']+=len(d)
assert len(trace)==8 and len(inv['video'])==8
assert all(m['audio_peak']==0 for m in inv['video'].values())
assert len(visual['02']['events'])==30
assert visual['02']['offset_spread_ms']<40
assert visual['03']['b2_gr_max_db']==0
for plugin in ['ORIG','PONTE']:
    assert len(audio['04'][plugin]['events'])==3
    for event in audio['04'][plugin]['events']:assert 0<event['t50']<event['t10']<2
    for event in visual['04'][plugin]['events']:assert .5<event['t90_to_t10_s']<1.2
    assert visual['05'][plugin]['b2_gr_max']==0 and visual['05'][plugin]['b3_gr_max']==0
    for stem,expected,segments in [
        ('04_GR_BAND3_2000Hz-'+plugin+'-ratio11',[2000],[(5,6)]),
        ('05_FUNDAMENTAL_PROBE_90_120_180Hz-'+plugin+'-BAND23',[90,120,180],[(4.05,4.55),(9.97,10.47),(15.90,16.40)])]:
        sr,x,_=read(ROOT/'audio'/(stem+'.wav'));observed=[]
        for frequency,(start,end) in zip(expected,segments):
            a=x[round(start*sr):round(end*sr),0]
            spectrum=np.abs(np.fft.rfft(a*np.hanning(len(a)),n=sr*4))
            measured=float(np.argmax(spectrum)/4);observed.append(measured)
            assert abs(measured-frequency)<=.5,(stem,frequency,measured)
        result['carrier_frequency_checks'][stem]=observed
for p in ROOT.glob('*.py'):compile(p.read_text(encoding='utf-8'),str(p),'exec')
result['status']='PASS: source hashes, all eight video traces, 30 A2 events, three GR tails, neutral 05, carriers and script syntax'
(OUT/'validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(result['status']);print('Inputs:',len(result['unchanged_input_hashes']),'new frames:',result['new_video_frames'])
