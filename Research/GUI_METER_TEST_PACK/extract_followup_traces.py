"""Frame-by-frame meter extraction for the new, separately positioned windows."""
from pathlib import Path
import sys,subprocess,json,re
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
from analyze_followup import ROOT,OUT,EXE,new_videos

def rois(stem):
    if 'ORIG' in stem:
        rows=[('main',891,393,583,'yellow'),('b2_io',1442,627,233,'yellow'),
            ('b2_gr',1442,605,233,'yellow'),('b3_io',1442,726,233,'yellow'),('b3_gr',1442,704,233,'yellow')]
    else:
        if stem.startswith('02'): x,y,w,mx,my,mw=1493,578,347,1619,165,230
        elif stem.startswith('03'): x,y,w,mx,my,mw=1476,564,347,1602,151,230
        elif stem.startswith('04') and 'B0' in stem: x,y,w,mx,my,mw=1454,572,347,1580,159,230
        elif stem.startswith('04'): x,y,w,mx,my,mw=1386,579,360,1520,166,238
        else: x,y,w,mx,my,mw=1352,588,360,1486,175,238
        rows=[('main',mx,my,mw,'yellow'),('b2_io',x,y,w,'green'),('b2_out',x,y+22,w,'green'),
            ('b2_gr',x,y+44,w,'red'),('b3_io',x,y+110,w,'orange'),
            ('b3_out',x,y+132,w,'orange'),('b3_gr',x,y+154,w,'red')]
    return rows+[('play',790,57,9,'green')]

def mask(a,c):
    r,g,b=a.astype(float).transpose(2,0,1)
    if c=='yellow':return (r>150)&(g>155)&(b<145)
    if c=='green':return (g>95)&(g>r*1.25)&(g>b*1.08)
    if c=='orange':return (r>160)&(g>70)&(r>g*1.2)&(b<130)
    return (r>155)&(r>g*1.35)&(r>b*1.2)

def extract():
    result={}
    for path in new_videos():
        rows=rois(path.stem)
        filters=[f'[0:v]format=rgb24,split={len(rows)}'+''.join(f'[s{i}]' for i in range(len(rows)))]
        for i,(_,x,y,w,c) in enumerate(rows):filters.append(f'[s{i}]crop={w}:3:{x}:{y}:exact=1,pad=600:3:0:0:black[r{i}]')
        filters.append(''.join(f'[r{i}]' for i in range(len(rows)))+f'vstack=inputs={len(rows)},format=rgb24,showinfo[v]')
        lp=OUT/(path.stem+'_frames.log'); records=[]
        with lp.open('w') as log:
            p=subprocess.Popen([EXE,'-hide_banner','-nostats','-i',str(path),'-filter_complex',';'.join(filters),
                '-map','[v]','-an','-fps_mode','passthrough','-f','rawvideo','-pix_fmt','rgb24','pipe:1'],stdout=subprocess.PIPE,stderr=log)
            size=600*3*len(rows)*3
            while True:
                raw=p.stdout.read(size)
                if not raw:break
                assert len(raw)==size
                frame=np.frombuffer(raw,np.uint8).reshape(len(rows)*3,600,3); record=[]
                for i,(_,x,y,w,c) in enumerate(rows):
                    active=np.where(mask(frame[i*3:(i+1)*3,:w],c).sum(axis=0)>=2)[0]
                    record += [int(active[0]) if len(active) else -1,int(active[-1]+1) if len(active) else 0,len(active)]
                records.append(record)
            assert p.wait()==0,lp.read_text()[-1000:]
        times=np.array([float(t) for t in re.findall(r'\bn:\s*\d+\s+pts:\s*-?\d+\s+pts_time:([\d.e+-]+)',lp.read_text())])
        assert len(times)==len(records)
        header=['time_s']+[n+'_'+s for n,*_ in rows for s in ['left','right','count']]
        np.savetxt(OUT/(path.stem+'_pixels.csv'),np.c_[times,records],delimiter=',',header=','.join(header),comments='',fmt='%.6f')
        dt=np.diff(times)
        result[path.name]={'rois':rows,'frames':len(times),'last_pts':float(times[-1]),
            'max_interval':float(np.max(dt)),'gaps_start_pts':times[:-1][dt>.025].tolist()}
        (OUT/'trace_inventory.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        print(path.name,len(times),flush=True)
if __name__=='__main__':extract()
