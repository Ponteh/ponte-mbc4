"""Independent, deterministic stimuli and explicit original-plugin render plan.

Python 3 + NumPy. No production DSP and no third-party recordings.
All files last exactly 60 seconds, including native sample-rate probes.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import math
import wave

import numpy as np

ROOT = Path(__file__).resolve().parent
DURATION = 60
RAMP_SECONDS = .002
SEED = 20260917


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def db(value):
    return 20 * math.log10(max(float(value), 1e-15))


def carrier(t, frequency):
    return np.sin(2 * np.pi * frequency * t)


def make_signal(number, sr):
    n = DURATION * sr
    t = np.arange(n, dtype=np.float64) / sr
    envelope = np.zeros(n)
    events = []
    ramp = round(RAMP_SECONDS * sr)

    def level(start, end, level_db, label, hz=315):
        a, b = round(start * sr), round(end * sr)
        target = 10 ** (level_db / 20)
        envelope[a:b] = target
        events.append(dict(start_s=start, end_s=end, label=label,
                           target_peak_dbfs=level_db, carrier_hz=hz))

    def smooth(values):
        # Causal half-cosine ramp at each boundary; event time = ramp START.
        values = values.copy()
        boundaries = np.flatnonzero(np.diff(values) != 0) + 1
        for a in boundaries:
            stop = min(a + ramp, len(values))
            old, new = values[a - 1], values[a]
            u = np.arange(stop - a) / ramp
            values[a:stop] = old + (new - old) * (.5 - .5 * np.cos(np.pi * u))
        return values

    frequency = {1:315, 5:50, 6:2000, 7:14000}.get(number, 315)
    if number in (1, 5, 6, 7):
        level(3, 55, -42, 'residual_baseline', frequency)
        for start, length, peak in [(6, .1, -6), (12, .5, -6), (19, 3, -6),
                                    (30, 3, -18), (40, 3, -12), (49, 1, -6)]:
            level(start, start + length, peak, 'compression_step', frequency)
        mono = carrier(t, frequency) * smooth(envelope)
    elif number == 2:
        level(3, 55, -42, 'residual_baseline')
        for start, gap in [(6, .05), (14, .25), (23, 1), (34, 3)]:
            level(start, start + .25, -6, 'pair_first')
            level(start + .25 + gap, start + .5 + gap, -6, 'pair_second')
        for i in range(8):
            level(46 + i * .6, 46.05 + i * .6, -6, 'pulse_train')
        mono = carrier(t, 315) * smooth(envelope)
    elif number in (3, 4):
        mono = np.zeros(n)
        for start, duty in [(3, 1), (12, .5), (21, .2), (30, .5), (39, 1), (48, .2)]:
            end = start + 6
            a, b = round(start * sr), round(end * sr)
            local = np.arange(b - a) / sr
            pulse = ((local % .25) < .25 * duty).astype(float)
            pulse = smooth(pulse)
            u = carrier(t[a:b], 2000) * pulse
            edge = min(ramp, len(u) // 2)
            fade = .5 - .5 * np.cos(np.pi * np.arange(edge) / edge)
            u[:edge] *= fade
            u[-edge:] *= fade[::-1]
            scale = (10 ** (-20 / 20) / np.sqrt(np.mean(u*u)) if number == 3
                     else 10 ** (-8 / 20) / np.max(np.abs(u)))
            mono[a:b] = u * scale
            events.append(dict(start_s=start, end_s=end, label='crest_segment',
                               carrier_hz=2000, duty=duty, modulation_hz=4,
                               normalisation='RMS -20 dBFS' if number == 3 else 'peak -8 dBFS'))
    elif number in (8, 9, 15, 18):
        for start, end, peak in [(3, 8, -30), (8, 12, -6), (12, 19, -36),
                                  (19, 19.3, -9), (19.3, 26, -36), (26, 30, -15),
                                  (30, 37, -36), (37, 41, -6), (41, 48, -36),
                                  (48, 50, -12), (50, 55, -36)]:
            level(start, end, peak, 'program_envelope', 0)
        if number in (8, 18):
            mono = sum(carrier(t, f) for f in (50, 315, 2000, 14000)) / 4
        elif number == 9:
            mono = np.random.default_rng(SEED).standard_normal(n)
            mono /= np.max(np.abs(mono))
        else:
            # Held-out synthetic phrase; explicitly not a real vocal recording.
            phase = 2 * np.pi * (137*t + .75/3 * np.sin(2*np.pi*3*t))
            mono = sum(np.sin(k*phase) / k for k in range(1, 13))
            mono /= np.max(np.abs(mono))
        mono *= smooth(envelope)
    elif number in (10, 11):
        if number == 10:
            level(3, 55, -18, 'program_constant')
            envelope[round(25*sr):round(31*sr)] = 0
            events.append(dict(start_s=25, end_s=31, label='program_silent_key_only'))
        else:
            level(3, 55, -42, 'key_residual')
            for a, b in [(6,9), (15,15.3), (24,30), (35,39), (46,47)]:
                level(a,b,-6,'external_key_high')
        mono = carrier(t,315)*smooth(envelope)
    elif number == 12:
        level(3, 55, -42, 'residual_baseline')
        for a in (6,18,30,42):
            level(a,a+3,-6,'stereo_compression_step')
        mono = carrier(t,315)*smooth(envelope)
        stereo = np.column_stack((mono,mono))
        # Channel changes occur on zero crossings of the 315 Hz carrier.
        stereo[:15*sr,1] = 0
        stereo[15*sr:27*sr,0] = 0
        stereo[39*sr:,1] *= -1
        for a,b,label in [(3,15,'L_only'), (15,27,'R_only'),
                          (27,39,'L_equals_R'),(39,55,'L_equals_minus_R')]:
            events.append(dict(start_s=a,end_s=b,label=label))
        return stereo, events
    elif number == 13:
        level(3,55,-18,'automation_carrier')
        for a in (6,16,26,36,46):
            level(a,a+2,-6,'automation_probe')
        mono = carrier(t,315)*smooth(envelope)
    elif number == 14:
        mono = np.zeros(n)
        for start, f0 in [(3,90),(20,120),(37,180)]:
            a,b = start*sr,(start+16)*sr
            phase = 2*np.pi*f0*t[a:b]
            harmonic = np.sin(phase) + sum(.2/k*np.sin(k*phase) for k in range(2,13))
            harmonic /= np.max(np.abs(harmonic))
            mono[a:b] = harmonic
            level(start,start+16,-42,'fundamental_baseline',f0)
            for offset,length,peak in [(2,.6,-9),(5,.25,-15),(8,2,-6),(12,.5,-12)]:
                level(start+offset,start+offset+length,peak,'fundamental_phrase',f0)
        mono *= smooth(envelope)
    elif number == 16:
        for a, f in [(3,315),(29,2000)]:
            for i, peak in enumerate((-36,-24,-12,-6,-12,-24,-36)):
                level(a+i*3,a+(i+1)*3,peak,'meter_plateau',f)
        mono = np.where(t<27,carrier(t,315),carrier(t,2000))*smooth(envelope)
    elif number == 17:
        for a,f in [(3,315),(29,2000)]:
            cursor = a
            for length in (.01,.03,.1,.3,1):
                for gap in (1.113,1.271):
                    level(cursor,cursor+length,-6,'meter_burst',f)
                    cursor += length + gap
        mono = np.where(t<27,carrier(t,315),carrier(t,2000))*smooth(envelope)
    else:
        raise ValueError(number)
    return np.column_stack((mono,mono)), events


NAMES = {1:'STEP_315',2:'MEMORY_315',3:'CREST_RMS_2000',4:'CREST_PEAK_2000',
         5:'STEP_50',6:'STEP_2000',7:'STEP_14000',8:'MULTITONE',9:'NOISE_HOLDOUT',
         10:'PROGRAM_SC_315',11:'KEY_SC_315',12:'STEREO_315',13:'AUTOMATION_315',
         14:'FUNDAMENTAL_90_120_180',15:'PHRASE_HOLDOUT',16:'METER_LEVELS',
         17:'METER_BURSTS',18:'RATE_PROBE'}


def source_name(number,sr=48000):
    return f'{number:02}_{NAMES[number]}_{sr}Hz_60s.wav'


def write_wav(path, data, sr):
    assert data.shape == (DURATION*sr,2) and np.isfinite(data).all()
    assert np.max(np.abs(data)) < .999
    integers = np.rint(data*8388607).astype(np.int32).reshape(-1)
    packed = np.empty((len(integers),3),dtype=np.uint8)
    for byte in range(3):
        packed[:,byte] = (integers >> (8*byte)) & 255
    with wave.open(str(path),'wb') as w:
        w.setparams((2,3,sr,0,'NONE','not compressed'))
        w.writeframes(packed.tobytes())
    with wave.open(str(path),'rb') as w:
        assert (w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes()) == (2,3,sr,sr*DURATION)
        raw = np.frombuffer(w.readframes(w.getnframes()),dtype=np.uint8).reshape(-1,3)
    decoded = raw[:,0].astype(np.int32) | raw[:,1].astype(np.int32)<<8 | raw[:,2].astype(np.int32)<<16
    decoded = ((decoded ^ 0x800000)-0x800000).reshape(-1,2)/8388607
    assert np.max(np.abs(decoded-data)) <= .51/8388607
    assert np.all(decoded[:3*sr] == 0) and np.all(decoded[56*sr:] == 0)
    return decoded


def build_plan():
    rows=[]
    def add(stage,number,profile='AUTO',solo='B2',model='MC404',sr=48000,
            overrides=None,automation=None,sc=False,repeat=False):
        cfg=dict(mode='AUTO',ratio=2,threshold_db=-27.5,knee=0,bite=1,
                 attack_ms=2.5,release_ms=250,gain_db=0)
        if profile=='B0':cfg.update(mode='R1',ratio=1)
        elif profile in ('R1','R2'):cfg['mode']=profile
        if overrides:cfg.update(overrides)
        bands={'MC202':2,'MC303':3,'MC404':4}[model]
        cross={'MC202':[785],'MC303':[100,785],'MC404':[100,785,10000]}[model]
        ident=f'T{len(rows)+1:03}'
        name=f'{ident}_ORIG_{model}_{number:02}_{solo}_{profile}_{sr}Hz_take1.wav'
        rows.append(dict(id=ident,stage=stage,source=source_name(number,sr),sample_rate=sr,
                         duration_s=60,expected_frames=60*sr,model=model,crossover_hz=cross,
                         input_db=0,output_db=0,phase_invert=False,link='UNLINKED',
                         IN=[True]*bands,SOLO=[] if solo=='ALL' else [int(s) for s in solo[1:].split('+')],
                         band_settings=[dict(band=i+1,**cfg) for i in range(bands)],
                         sidechain=source_name(11) if sc else None,
                         automation=automation,repeat_fresh_instance=repeat,
                         output=name,profile=profile))
    for num,solo in [(1,'B2'),(2,'B2'),(6,'B3')]:
        for prof in ('B0','AUTO','R1','R2'):add('P0_AUTO_PRIMA',num,prof,solo)
    for num,solo in [(3,'B3'),(4,'B3'),(5,'B1'),(7,'B4'),(8,'ALL'),(9,'ALL'),(15,'ALL')]:
        for prof in ('B0','AUTO'):add('P1_AUTO_ESTESO',num,prof,solo)
    for num,solo in [(1,'B2'),(6,'B3')]:
        for bite in (5,10):add('P2_CONTROLLI',num,f'AUTO_B{bite}',solo,overrides={'bite':bite})
    for label,change in [('R4',{'ratio':4}),('TH18',{'threshold_db':-18}),
                         ('KM5',{'knee':-5}),('KP10',{'knee':10})]:
        add('P2_CONTROLLI',1,'AUTO_'+label,overrides=change)
    add('P2_CONTROLLI',1,'AUTO_MAN_LOW',overrides={'attack_ms':.25,'release_ms':25})
    add('P2_CONTROLLI',1,'AUTO_MAN_HIGH',overrides={'attack_ms':25,'release_ms':2500})
    add('P2_CONTROLLI',1,'AUTO_REPEAT',repeat=True)
    for model,solo in [('MC202','B1'),('MC303','B2')]:
        for num,selected in [(1,solo),(8,'ALL')]:
            for prof in ('B0','AUTO'):add('P3_MODELLI',num,prof,selected,model)
    for prof in ('B0','AUTO','R1'):add('P4_STEREO',12,prof)
    for prof in ('B0','AUTO','R1','R2'):
        add('P5_SIDECHAIN_CONDIZIONALE',10,prof+'_EXT',sc=True,
            overrides={'mode':'R1' if prof=='B0' else prof,'ratio':1 if prof=='B0' else 2})
    for prof in ('B0','AUTO'):add('P5_SIDECHAIN_CONDIZIONALE',10,prof)
    for prof in ('B0','AUTO'):add('P6_AUTOMAZIONE',13,prof)
    add('P6_AUTOMAZIONE',13,'AUTO_MAN_MOVE',automation='MANUAL_TIMES')
    add('P6_AUTOMAZIONE',13,'MODE_SWITCH',automation='MODES')
    add('P6_AUTOMAZIONE',13,'THRESH_MOVE',automation='THRESHOLD')
    add('P6_AUTOMAZIONE',13,'B0_XOVER',overrides={'mode':'R1','ratio':1},automation='CROSSOVER')
    add('P6_AUTOMAZIONE',13,'AUTO_XOVER',automation='CROSSOVER')
    for sr in (44100,48000,88200,96000,192000):
        for solo in ('ALL','B1'):
            for prof in ('B0','AUTO'):add('P7_SAMPLE_RATE',18,prof,solo,sr=sr)
    for prof,over in [('B0',None),('R1_250',{'mode':'R1'}),('R1_500',{'mode':'R1','release_ms':500})]:
        add('P8_ESPERTO',14,prof,'B2+3',overrides=over)
    for num,solo in [(1,'B2'),(6,'B3')]:
        add('P8_ESPERTO',num,'R1_500',solo,overrides={'mode':'R1','release_ms':500})
    for num in (16,17):add('P9_METER',num,'B0','ALL')
    for row in rows:
        matches=[r for r in rows if r['band_settings'][0]['ratio']==1
                 and all(r[k]==row[k] for k in ('source','model','sample_rate','SOLO','sidechain'))
                 and (r['automation']=='CROSSOVER')==(row['automation']=='CROSSOVER')]
        assert len(matches)==1, (row['id'],[m['id'] for m in matches])
        row['neutral_reference_id']=matches[0]['id']
    return rows


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--force',action='store_true',help='Regenerate existing SOURCES only; renders are never changed.')
    ap.add_argument('--plan-only',action='store_true',help='Regenerate the render plan/log using the existing source manifest.')
    args=ap.parse_args()
    (ROOT/'audio').mkdir(exist_ok=True)
    (ROOT/'renders').mkdir(exist_ok=True)
    manifest=dict(pack_version=1,duration_s=60,format='PCM24 stereo',seed=SEED,
                  ramp_s=RAMP_SECONDS,events='start = start of causal amplitude ramp; overlapping baseline/events are intentional',files=[])
    if args.plan_only:
        manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
    for number in ([] if args.plan_only else range(1,19)):
        rates=(44100,48000,88200,96000,192000) if number==18 else (48000,)
        for sr in rates:
            path=ROOT/'audio'/source_name(number,sr)
            if path.exists() and not args.force:
                raise SystemExit('Sources already exist. Use --force only to regenerate known sources. '+str(path))
            x,events=make_signal(number,sr)
            y=write_wav(path,x,sr)
            for event in events:
                a,b=round(event['start_s']*sr),round(event['end_s']*sr)
                event.update(start_sample=a,end_sample=b,
                             measured_peak_dbfs=db(np.max(np.abs(y[a:b]))),
                             measured_rms_dbfs=db(np.sqrt(np.mean(y[a:b]**2))))
            for event in events:
                if event['label']=='crest_segment':
                    metric=event['measured_rms_dbfs'] if number==3 else event['measured_peak_dbfs']
                    assert abs(metric-(-20 if number==3 else -8)) < .001
            if number!=12:assert np.array_equal(y[:,0],y[:,1])
            if number==12:
                assert not np.any(y[:15*sr,1]) and not np.any(y[15*sr:27*sr,0])
                assert np.array_equal(y[39*sr:,0],-y[39*sr:,1])
            manifest['files'].append(dict(file=path.name,sample_rate=sr,frames=60*sr,
                  duration_s=60,channels=2,bytes=path.stat().st_size,sha256=digest(path),
                  peak_dbfs=db(np.max(np.abs(y))),events=events,
                  fit_role='held_out' if number in (9,15) else 'calibration_or_control'))
            print('Verified',path.name,flush=True)
    (ROOT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    rows=build_plan()
    (ROOT/'render_plan.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
    with (ROOT/'RENDER_LOG.csv').open('w',newline='',encoding='utf-8-sig') as f:
        fields=['id','output','done','actual_plugin_version','actual_sample_rate','buffer',
                'export_or_print','preset_or_screenshot','automation_verified','notes']
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
        for row in rows:w.writerow(dict(id=row['id'],output=row['output'],buffer=512,export_or_print='export offline'))
    lines=['# Piano completo dei render originali','',
           'Generato da `generate.py`. Ogni riga = un export stereo di **60.000 s**.',
           'Impostare tutti i valori indicati su **tutte le bande**; IN tutti ON.',
           'Input/Output/Gain 0 dB, fase normale, LINK UNLINKED; leggere prima README.md.',
           'B0 = riferimento ratio 1:1. AUTO = pulsante A/Auto ON; R1/R2 = Auto OFF e relativa legge.',
           'Xover MC404: 100/785/10000 Hz; MC303: 100/785 Hz; MC202: 785 Hz.',
           'I numeri sorgente corrispondono alla tabella audio nel README; il nome completo e ogni parametro sono in render_plan.json.','']
    stage=None
    for row in rows:
        if stage!=row['stage']:
            stage=row['stage'];lines += ['', '## '+stage,'',
              '| ID | Sorgente | Modello / Hz | SOLO | TC | COMP | THRESH dB | KNEE | BITE | ATT ms | REL ms | SC / automazione | B0 di riferimento |',
              '| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |']
        c=row['band_settings'][0]
        lines.append(f"| {row['id']} | {row['source'][:2]} | {row['model']} / {row['sample_rate']} | {row['SOLO'] or 'nessuno'} | {c['mode']} | {c['ratio']}:1 | {c['threshold_db']} | {c['knee']} | {c['bite']} | {c['attack_ms']} | {c['release_ms']} | {('EXT 11' if row['sidechain'] else 'interna')+' / '+(row['automation'] or 'nessuna')} | {row['neutral_reference_id']} |")
    lines += ['','## Nomi esatti degli export','', '| ID | Nome file |','| --- | --- |']
    lines += [f"| {r['id']} | `{r['output']}` |" for r in rows]
    lines += ['','Totale: '+str(len(rows))+' export; ciascuno dura 60 secondi. La sidechain esterna e condizionata alla disponibilita effettiva.','']
    (ROOT/'RENDER_PLAN.md').write_text('\n'.join(lines),encoding='utf-8')
    print('DONE:',len(manifest['files']),'sources;',len(rows),'planned exports;',
          round(sum(f['bytes'] for f in manifest['files'])/1024**2,1),'MiB')


if __name__=='__main__':main()
