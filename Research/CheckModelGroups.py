"""Tone-wise plateau attenuation for the two/three-band diagnostic cases."""
from pathlib import Path
import json
import numpy as np
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parent
PACK = ROOT / 'NEXT_RELEASE_ORIGINAL_TEST_PACK'
WORK = ROOT.parents[2] / 'build/mc2000-nap-2026-09-23/model9'
old = json.loads((WORK / 'model8_outputs.json').read_text())
rows = []
for neutral, compressed in [('T040', 'T041'), ('T044', 'T045')]:
    for model in ['original', 'model8', 'model9']:
        signals = []
        for key in [neutral, compressed]:
            if model == 'original':
                rate, x = wavfile.read(PACK / 'renders' / (key + '.wav'))
                assert rate == 48000 and x.dtype.kind == 'f'
            else:
                x = np.fromfile(old[key] if model == 'model8' else WORK / (key + '.f32'), dtype='<f4').reshape(-1, 2)
            signals.append(x)
        for start, stop in [(9, 11), (37.5, 40.5)]:
            a, b = (x[int(start*48000):int(stop*48000), 0].astype(float) for x in signals)
            t = np.arange(len(a)) / 48000
            for frequency in [50, 315, 2000, 14000]:
                carrier = np.exp(-2j*np.pi*frequency*t)
                gr = 20*np.log10(abs(a @ carrier)/abs(b @ carrier))
                rows.append(dict(test=compressed, model=model, start_s=start, frequency_hz=frequency, attenuation_db=float(gr)))
dest = PACK / 'analysis_2026-09-24/model_groups_tones.json'
dest.write_text(json.dumps(rows, indent=2)+'\n')
for row in rows:
    print(row)
