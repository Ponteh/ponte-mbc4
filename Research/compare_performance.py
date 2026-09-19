"""Compare paired PerformanceBenchmark runs; audio equality is mandatory.

Usage: python compare_performance.py baseline-directory current-directory output.json
Run executables sequentially on the same machine, not simultaneously.
"""
from pathlib import Path
import array
import collections
import csv
import hashlib
import json
import statistics
import sys


def read_timings(folder):
    grouped = collections.defaultdict(list)
    with (folder / 'timing.csv').open(newline='') as f:
        for row in csv.DictReader(f):grouped[int(row['case'])].append(row)
    assert len(grouped)==54 and all(len(rows)==5 for rows in grouped.values())
    return grouped


def compare(a, b):
    baseline, current = read_timings(a), read_timings(b)
    results=[]
    for case, rows in baseline.items():
        x=(a/f'{case}.f32').read_bytes();y=(b/f'{case}.f32').read_bytes()
        assert len(x)==len(y)
        identical=x==y
        ax=array.array('f');ax.frombytes(x)
        ay=array.array('f');ay.frombytes(y)
        error=max(abs(p-q) for p,q in zip(ax,ay))
        before=statistics.median(float(r['total_us']) for r in rows)
        after=statistics.median(float(r['total_us']) for r in current[case])
        results.append(dict(case=case,mode=rows[0]['mode'],rate=rows[0]['rate'],
            block=rows[0]['block'],automation=rows[0]['automation'],bit_identical=identical,
            max_abs_error=error,output_sha256=hashlib.sha256(y).hexdigest(),
            baseline_median_us=before,current_median_us=after,saving_percent=100*(1-after/before),
            baseline_p99_us=statistics.median(float(r['p99_us']) for r in rows),
            current_p99_us=statistics.median(float(r['p99_us']) for r in current[case])))
    return dict(scope='DSP process only; 54 deterministic cases, 5 repeats after warm-up. Not a DAW CPU or GUI benchmark.',
        all_bit_identical=all(r['bit_identical'] for r in results),
        median_saving_percent=statistics.median(r['saving_percent'] for r in results),
        min_saving_percent=min(r['saving_percent'] for r in results),
        max_saving_percent=max(r['saving_percent'] for r in results),cases=results)


if __name__=='__main__':
    a,b,out=map(Path,sys.argv[1:])
    result=compare(a,b)
    out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='cases'},indent=2))
    raise SystemExit(0 if result['all_bit_identical'] else 1)
