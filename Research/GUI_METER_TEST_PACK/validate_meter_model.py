"""Engineering check of the GR display model; standard library only.

03 selects a rounded smoothing time from normalized return intervals; 04 is
held out. Export-derived GR is a 10 ms RMS surrogate, not the per-block
detector value or audio synchronized with OBS. This is not a new capture.
"""
from pathlib import Path
import csv
import json
import math
import re

ROOT = Path(__file__).resolve().parent
OLD = ROOT / 'analysis_2026-09-15'
NEW = ROOT / 'analysis_2026-09-15_take2_04_05'
OUT = ROOT / 'meter_fix_2026-09-15'
OUT.mkdir(exist_ok=True)
header = (ROOT.parents[1] / 'Source/UI/MeterBallistics.h').read_text()
taus = re.findall(r'smoothingSeconds = ([\d.]+)', header)
tau = float(taus[1])
old = json.loads((OLD / 'measurements.json').read_text())
new = json.loads((NEW / 'video_measurements.json').read_text())


def audio(path):
    rows = list(csv.DictReader(path.open()))
    t = [float(r['time_s']) for r in rows]
    values = [float(r['gr_db']) for r in rows]
    return t, [max(0.0, v) if math.isfinite(v) else 0.0 for v in values]


def simulate(t, values, smoothing):
    result = []
    y = 0.0
    for i, target in enumerate(values):
        dt = t[i] - t[i - 1] if i else .01
        y = target if target >= y or smoothing == 0 else target + (y - target) * math.exp(-dt / smoothing)
        result.append(y)
    return result


def intervals(t, values, releases):
    result = []
    for release in releases:
        peak = max(v for u, v in zip(t, values) if release - .07 < u < release + .02)
        crossings = [next(u for u, v in zip(t, values) if release + .02 < u < release + 2 and v <= f * peak)
                     for f in [.9, .5, .1]]
        result.append({'t90_to_t50_s': crossings[1] - crossings[0],
                       't90_to_t10_s': crossings[2] - crossings[0]})
    return result


source03 = audio(OLD / 'audio_gr_PONTE.csv')
source04 = audio(NEW / '04_audio_gr_PONTE.csv')
orig03 = old['gr_visual']['ORIG']['events']
orig04 = new['04']['ORIG']['events']
releases03 = [e['wav_release_s'] for e in orig03]
releases04 = [e['wav_release'] for e in orig04]
target03 = [{'t90_to_t50_s': e['t50_seconds_relative_to_aligned_wav'] - e['t90_seconds_relative_to_aligned_wav'],
             't90_to_t10_s': e['t90_to_t10_seconds']} for e in orig03]
target04 = [{'t90_to_t50_s': e['t50_transport_relative'] - e['t90_transport_relative'],
             't90_to_t10_s': e['t90_to_t10_s']} for e in orig04]


def error(predicted, target):
    return math.sqrt(sum((p[key] - t[key]) ** 2 for p, t in zip(predicted, target)
                         for key in p) / (2 * len(target)))


grid = []
for ms in range(0, 301, 10):
    predicted = intervals(source03[0], simulate(*source03, ms / 1000), releases03)
    grid.append({'tau_ms': ms, 'interval_rmse_s': error(predicted, target03)})

result = {'method': __doc__, 'level_fall_db_per_s': 14.3, 'level_smoothing_s': float(taus[0]),
          'gr_smoothing_s': tau, '03_grid': grid, '03_best_grid': min(grid, key=lambda x: x['interval_rmse_s'])}
for name, source, releases, target in [('03', source03, releases03, target03), ('04', source04, releases04, target04)]:
    before = intervals(source[0], source[1], releases)
    after = intervals(source[0], simulate(*source, tau), releases)
    result[name] = {'original_video': target, 'raw_audio_surrogate': before,
                    'smoothed_audio_surrogate': after, 'before_interval_rmse_s': error(before, target),
                    'after_interval_rmse_s': error(after, target)}
    assert result[name]['after_interval_rmse_s'] < result[name]['before_interval_rmse_s']
    assert all(.83 < e['t90_to_t10_s'] < .94 for e in after)

(OUT / 'model_validation.json').write_text(json.dumps(result, indent=2, allow_nan=False) + '\n', encoding='utf-8')
print(json.dumps({k: v for k, v in result.items() if k != '03_grid'}, indent=2))
