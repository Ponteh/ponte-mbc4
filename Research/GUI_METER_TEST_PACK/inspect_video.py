"""Extract inventory and one native frame from each submitted video."""
from pathlib import Path
import subprocess
import json
import hashlib
import imageio_ffmpeg

ROOT = Path(__file__).resolve().parent
OUT = ROOT/'analysis_2026-09-15'
OUT.mkdir(exist_ok=True)
exe = imageio_ffmpeg.get_ffmpeg_exe()
inventory = {}
for path in sorted((ROOT/'audio').glob('*.mp4')):
    if not path.name.startswith(('01_', '02_', '03_')) or not path.stem.endswith('_take1'):
        continue
    decoder = imageio_ffmpeg.read_frames(str(path))
    meta = next(decoder)
    decoder.close()
    inventory[path.name] = {**meta, 'bytes':path.stat().st_size,
        'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
    log = subprocess.run([exe, '-hide_banner', '-i', str(path)], capture_output=True, text=True).stderr
    (OUT/(path.stem+'_metadata.txt')).write_text(log, encoding='utf-8')
    subprocess.run([exe, '-v', 'error', '-ss', '8', '-i', str(path), '-frames:v', '1',
        '-y', str(OUT/(path.stem+'_8s.png'))], check=True)
(OUT/'video_inventory.json').write_text(json.dumps(inventory, indent=2), encoding='utf-8')
print(json.dumps(inventory, indent=2))
