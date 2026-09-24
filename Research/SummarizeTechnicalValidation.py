"""Summarise local diagnostic measurements. Uses Python standard library only."""
from pathlib import Path
import csv,json,statistics
ROOT=Path(__file__).resolve().parent/'technical_validation_2026-09-24'
def read(name):
    with (ROOT/name).open(encoding='utf-8-sig') as f:return list(csv.DictReader(f))
def med(rows,key):return statistics.median(float(r[key]) for r in rows)
gui=read('gui-profile.csv');callback=read('callback-profile.csv')
g=[]
names=['closed','hidden','idle','animated','resize']
for instances in [1,4]:
    for scenario in range(5):
        rows=[r for r in gui if int(r['instances'])==instances and int(r['scenario'])==scenario]
        assert len(rows)==5
        row=dict(instances=instances,scenario=names[scenario])
        for key in rows[0]:
            if key not in ['instances','scenario','repeat']:row[key]=med(rows,key)
        row['private_bytes_last_minus_first']=int(rows[-1]['private_bytes'])-int(rows[0]['private_bytes'])
        g.append(row)
c=[]
for rate in [48000,96000,192000]:
    for block in [64,512,2048]:
        for mode in [0,1,2]:
            for consumer in [0,1]:
                rows=[r for r in callback if tuple(int(r[k]) for k in ['rate','block','mode','consumer'])==(rate,block,mode,consumer)]
                assert len(rows)==5
                row=dict(rate=rate,block=block,mode=mode,consumer=consumer)
                for key in ['median_us','p99_us','max_us','deadline_us','snapshot_us','fifo_us','dsp_us']:row[key]=med(rows,key)
                row['p99_deadline_fraction']=row['p99_us']/row['deadline_us']
                row['median_us_min_max']=[min(float(r['median_us']) for r in rows),max(float(r['median_us']) for r in rows)]
                c.append(row)
result=dict(gui=g,callback=c,
    maximum_callback_p99_deadline_fraction=max(r['p99_deadline_fraction'] for r in c),
    callback_cases_p99_over_deadline=sum(r['p99_deadline_fraction']>1 for r in c),
    maximum_observed_block_deadline_fraction=max(float(r['max_us'])/float(r['deadline_us']) for r in callback),
    repetitions_with_observed_block_over_deadline=sum(float(r['max_us'])>float(r['deadline_us']) for r in callback),
    silent_gui_fft_calls=sum(r['fft_calls'] for r in g if r['scenario'] in ['closed','hidden','idle']),
    maximum_gui_private_growth_bytes=max(r['private_bytes_last_minus_first'] for r in g))
result['observed_overruns']=[dict(**{k:int(r[k]) for k in ['rate','block','mode','consumer','repeat']},
    wall_peak_to_median=float(r['max_us'])/float(r['median_us']),
    thread_cycles_peak_to_median=float(r['wall_peak_cycles'])/float(r['median_cycles']))
    for r in callback if float(r['max_us'])>float(r['deadline_us'])]
(ROOT/'summary.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
lines=['| Istanze | Scenario | CPU thread / passata (ms) | FFT / passata | Paint grafici (ms) | Variazione private bytes |',
       '| ---: | --- | ---: | ---: | ---: | ---: |']
for r in g:lines.append(f"| {r['instances']} | {r['scenario']} | {r['thread_cpu_ms']:.2f} | {r['fft_calls']:.0f} | {r['graph_paint_us']/1000:.2f} | {r['private_bytes_last_minus_first']} |")
(ROOT/'tables.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print({k:v for k,v in result.items() if k not in ['gui','callback']})
