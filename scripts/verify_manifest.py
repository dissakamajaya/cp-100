from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parents[1]
data = json.loads((root / 'docs/SOURCE-MANIFEST.json').read_text())
failures = []
for item in data['files']:
    path = root / item['destination']
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != item['packaged_sha256']:
        failures.append(item['destination'])
if failures:
    raise SystemExit('Files differ from initial handoff snapshot: ' + ', '.join(failures))
print(f"PASS: {len(data['files'])} source assets match the initial handoff snapshot")
