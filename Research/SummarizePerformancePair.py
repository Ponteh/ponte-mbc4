"""Summarise paired CPU observations without treating scheduler noise as DSP cost."""
from pathlib import Path
import csv, json, statistics
ROOT = Path(__file__).resolve().parent / 'validation_2026-09-23-nap'
with (ROOT/'performance_paired.csv').open(encoding='utf-8-sig') as f:
    rows = list(csv.DictReader(f))
cases = []
for case in sorted({int(r['case']) for r in rows}):
    group = [r for r in rows if int(r['case']) == case]
    med = lambda name: statistics.median(float(r[name]) for r in group)
    before, after = med('before_us'), med('after_us')
    cases.append(dict(case=case, rate=int(group[0]['rate']), block=int(group[0]['block']),
        mode=int(group[0]['mode']), automation=bool(int(group[0]['automation'])),
        saving_percent=100*(1-after/before), before_us=before, after_us=after,
        cycle_saving_percent=100*(1-med('after_cycles')/med('before_cycles')) if med('before_cycles') else None,
        before_p99_us=med('before_p99'), after_p99_us=med('after_p99'),
        bit_equal=all(r['bit_equal']=='1' for r in group),
        max_error=max(float(r['max_error']) for r in group)))
result=dict(method='54 cases, 5 repetitions after warm-up, alternating block order, same process, model9 both sides, FTZ/DAZ',
    median_saving_percent=statistics.median(r['saving_percent'] for r in cases),
    median_cycle_saving_percent=statistics.median(r['cycle_saving_percent'] for r in cases if r['cycle_saving_percent'] is not None),
    saving_range_percent=[min(r['saving_percent'] for r in cases),max(r['saving_percent'] for r in cases)],
    regressions_over_5_percent=[r['case'] for r in cases if r['saving_percent'] < -5],
    all_bit_equal=all(r['bit_equal'] for r in cases),cases=cases)
(ROOT/'performance_paired.json').write_text(json.dumps(result,indent=2)+'\n')
print({k:v for k,v in result.items() if k!='cases'})
