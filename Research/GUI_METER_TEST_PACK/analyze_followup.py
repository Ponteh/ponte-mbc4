"""September 15 follow-up batch: preserve the first analysis and all inputs."""
from pathlib import Path
import json, hashlib, subprocess, sys
import numpy as np
from scipy.io import wavfile
import imageio_ffmpeg
sys.path.insert(0,str(Path(__file__).resolve().parent))
from analyze_audio import read, envelope, db, high_events

ROOT=Path(__file__).resolve().parent
OLD=ROOT/'analysis_2026-09-15'
OUT=ROOT/'analysis_2026-09-15_take2_04_05'
OUT.mkdir(exist_ok=True)
EXE=imageio_ffmpeg.get_ffmpeg_exe()

def new_videos():
    return [p for p in sorted((ROOT/'audio').glob('*.mp4')) if 'take2' in p.name or p.name.startswith(('04_','05_'))]

def inventory():
    previous=json.loads((OLD/'audio_results.json').read_text())['files']
    result={'wav':{},'video':{}}
    for p in sorted((ROOT/'audio').glob('*.wav')):
        sr,x,dtype=read(p)
        h=hashlib.sha256(p.read_bytes()).hexdigest()
        result['wav'][p.name]=dict(sr=sr,duration=len(x)/sr,frames=len(x),channels=x.shape[1],dtype=dtype,
            peak_dbfs=float(db(np.max(np.abs(x)))),lr_difference=float(np.max(np.abs(x[:,0]-x[:,-1]))),
            sha256=h,unchanged_from_first_batch=(h==previous[p.name]['sha256']) if p.name in previous else None)
    for p in new_videos():
        decoder=imageio_ffmpeg.read_frames(str(p));meta=next(decoder);decoder.close()
        log=subprocess.run([EXE,'-hide_banner','-i',str(p)],capture_output=True,text=True).stderr
        (OUT/(p.stem+'_metadata.txt')).write_text(log,encoding='utf-8')
        for t in [2,8,16]:
            subprocess.run([EXE,'-v','error','-ss',str(t),'-i',str(p),'-frames:v','1','-y',
                str(OUT/(p.stem+f'_{t}s.png'))],check=True)
        raw=subprocess.run([EXE,'-v','error','-i',str(p),'-vn','-ac','1','-f','f32le','pipe:1'],capture_output=True,check=True).stdout
        a=np.frombuffer(raw,np.float32)
        result['video'][p.name]={**meta,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),
            'audio_peak':float(np.max(np.abs(a))),'audio_nonzero':int(np.count_nonzero(a))}
    (OUT/'inventory.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2),flush=True)

if __name__=='__main__':inventory()
