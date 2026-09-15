"""Decode native video timestamps and meter strips; never rewrite recordings.

ROIs were inspected on the native 8 s frames. Pixel endpoints are retained so
calibration can be revised without decoding again. No temporal interpolation.
"""
from pathlib import Path
import subprocess, json, re
import numpy as np
import imageio_ffmpeg
import sys

ROOT = Path(__file__).resolve().parent
OUT = ROOT/'analysis_2026-09-15'
EXE = imageio_ffmpeg.get_ffmpeg_exe()

def rois(stem):
    if 'ORIG' in stem:
        dx, dy = (-2, -59) if 'B1' in stem else ((-5,-10) if 'B0' in stem else (0,0))
        rows = [('main',976,420,583,'yellow'), ('main_r',976,435,583,'yellow'),
                ('b2_io',1527,654,233,'yellow'), ('b2_io_r',1527,661,233,'yellow'),
                ('b2_gr',1527,632,233,'yellow'), ('b3_io',1527,753,233,'yellow'),
                ('b3_gr',1527,731,233,'yellow')]
        rows = [(n,x+dx,y+dy,w,c) for n,x,y,w,c in rows]
    else:
        rows = [('main',1537,212,230,'yellow'), ('main_r',1537,230,230,'yellow'),
                ('b2_io',1411,625,347,'green'), ('b2_out',1411,647,347,'green'),
                ('b2_gr',1411,669,347,'red'), ('b3_io',1411,735,347,'orange'),
                ('b3_out',1411,757,347,'orange')]
    return rows + [('play',790,57,9,'green')]

def mask(a, colour):
    r,g,b = a.astype(float).transpose(2,0,1)
    if colour == 'yellow': return (r>150)&(g>155)&(b<145)
    if colour == 'green': return (g>95)&(g>r*1.25)&(g>b*1.08)
    if colour == 'orange': return (r>160)&(g>70)&(r>g*1.2)&(b<130)
    return (r>155)&(r>g*1.35)&(r>b*1.2)

inventory = {}
for path in sorted((ROOT/'audio').glob('*.mp4')):
    if not path.name.startswith(('01_', '02_', '03_')) or not path.stem.endswith('_take1'):
        continue
    if len(sys.argv)>1 and sys.argv[1] not in path.stem: continue
    rows = rois(path.stem)
    filters = [f'[0:v]format=rgb24,split={len(rows)}'+''.join(f'[s{i}]' for i in range(len(rows)))]
    for i,(_,x,y,w,c) in enumerate(rows):
        filters.append(f'[s{i}]crop={w}:3:{x}:{y}:exact=1,pad=600:3:0:0:black[r{i}]')
    filters.append(''.join(f'[r{i}]' for i in range(len(rows)))+f'vstack=inputs={len(rows)},format=rgb24,showinfo[v]')
    logpath = OUT/(path.stem+'_frames.log')
    data = []
    with logpath.open('w') as log:
        p = subprocess.Popen([EXE,'-hide_banner','-nostats','-i',str(path),'-filter_complex',';'.join(filters),
            '-map','[v]','-an','-fps_mode','passthrough','-f','rawvideo','-pix_fmt','rgb24','pipe:1'],
            stdout=subprocess.PIPE, stderr=log)
        size = 600*3*len(rows)*3
        while True:
            raw = p.stdout.read(size)
            if not raw: break
            if len(raw)!=size: raise RuntimeError('Incomplete video frame')
            frame = np.frombuffer(raw,np.uint8).reshape(3*len(rows),600,3)
            record = []
            for i,(_,x,y,w,c) in enumerate(rows):
                active = np.where(np.sum(mask(frame[i*3:(i+1)*3,:w],c),axis=0)>=2)[0]
                record += [int(active[0]) if len(active) else -1,
                           int(active[-1]+1) if len(active) else 0, len(active)]
            data.append(record)
        if p.wait()!=0: raise RuntimeError(logpath.read_text()[-2000:])
    times = np.array([float(t) for t in re.findall(r'\bn:\s*\d+\s+pts:\s*-?\d+\s+pts_time:([\d.e+-]+)',logpath.read_text())])
    if len(times)!=len(data): raise RuntimeError((len(times),len(data)))
    names = ['time_s']+[n+'_'+suffix for n,*_ in rows for suffix in ['left','right','count']]
    np.savetxt(OUT/(path.stem+'_pixels.csv'),np.c_[times,data],delimiter=',',header=','.join(names),comments='',fmt='%.6f')
    audio = subprocess.run([EXE,'-v','error','-i',str(path),'-vn','-ac','1','-f','f32le','pipe:1'],capture_output=True,check=True).stdout
    audio = np.frombuffer(audio,np.float32)
    inventory[path.name] = dict(rois=rows,frames=len(times),first_pts=float(times[0]),last_pts=float(times[-1]),
        min_frame_interval=float(np.min(np.diff(times))),max_frame_interval=float(np.max(np.diff(times))),
        audio_peak=float(np.max(np.abs(audio))),audio_nonzero_samples=int(np.count_nonzero(audio)))
    print(path.name, inventory[path.name],flush=True)
    (OUT/(path.stem+'_trace_inventory.json')).write_text(json.dumps(inventory[path.name],indent=2),encoding='utf-8')
(OUT/('trace_inventory'+('_'+sys.argv[1] if len(sys.argv)>1 else '')+'.json')).write_text(json.dumps(inventory,indent=2),encoding='utf-8')
