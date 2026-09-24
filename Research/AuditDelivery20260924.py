"""Audit the September 24 delivery, preserving earlier inventories and originals."""
from pathlib import Path
import sys,json,hashlib
ROOT=Path(__file__).resolve().parent/'NEXT_RELEASE_ORIGINAL_TEST_PACK'
sys.path.insert(0,str(ROOT))
from validate import wav_info
OUT=ROOT/'analysis_2026-09-24';OUT.mkdir(exist_ok=True)
old={r['file']:r for r in json.loads((ROOT/'analysis_2026-09-23-corrected/inventory.json').read_text())}
rows=[]
for path in sorted((ROOT/'renders').glob('*.wav')):
 raw=path.read_bytes();h=hashlib.sha256(raw).hexdigest()
 rows.append(dict(file=path.name,sha256=h,previous='new' if path.name not in old else 'unchanged' if old[path.name]['sha256']==h else 'replaced',**wav_info(raw)))
(OUT/'inventory.json').write_text(json.dumps(rows,indent=2)+'\n')
plan=json.loads((ROOT/'render_plan.json').read_text());byname={r['file']:r for r in rows}
summary=dict(count=len(rows),changed=[r for r in rows if r['previous']!='unchanged'],missing=[r['id'] for r in plan if r['id']+'.wav' not in byname],wrong_format=[r['id'] for r in plan if r['id']+'.wav' in byname and (byname[r['id']+'.wav']['sample_rate']!=r['sample_rate'] or byname[r['id']+'.wav']['duration_s']!=60 or byname[r['id']+'.wav']['all_silent'])],sidechain_note='User confirms original tested plugin has no external sidechain control; original external-key comparison is not executable. Internal Ponte routing tests remain required.')
(OUT/'delivery.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:v for k,v in summary.items() if k!='changed'},indent=2),flush=True)
print('Changed:',[r['file'] for r in summary['changed']],flush=True)
