"""Check the exported binary STL, including topology after float32 serialization."""
from pathlib import Path
import collections
import hashlib
import json
import math
import struct

root = Path(__file__).resolve().parents[2]
folder = root / 'enclosure/print-ready'
reports = []
for item in json.loads((folder / 'manifest.json').read_text()):
    path = folder / item['filename']
    raw = path.read_bytes()
    count, = struct.unpack_from('<I', raw, 80)
    assert len(raw) == 84 + 50 * count
    vertices, indices, edges, triangles = [], {}, collections.defaultdict(list), []
    for i in range(count):
        values = struct.unpack_from('<12fH', raw, 84 + 50 * i)
        assert all(math.isfinite(v) for v in values[:12])
        points = [tuple(values[j:j + 3]) for j in (3, 6, 9)]
        ids = []
        for p in points:
            if p not in indices:
                indices[p] = len(vertices)
                vertices.append(p)
            ids.append(indices[p])
        assert len(set(ids)) == 3, 'Degenerate triangle'
        triangles.append(ids)
        for a, b in zip(ids, ids[1:] + ids[:1]):
            edges[tuple(sorted((a, b)))].append((a, b, i))
    adjacency = [[] for _ in triangles]
    for uses in edges.values():
        assert len(uses) == 2, 'Open or non-manifold edge'
        a, b = uses
        assert a[:2] == b[1::-1], 'Inconsistent winding'
        adjacency[a[2]].append(b[2])
        adjacency[b[2]].append(a[2])
    remaining, components = set(range(count)), []
    while remaining:
        stack, face_ids = [remaining.pop()], []
        while stack:
            face = stack.pop()
            face_ids.append(face)
            for other in adjacency[face]:
                if other in remaining:
                    remaining.remove(other)
                    stack.append(other)
        points = [vertices[v] for face in face_ids for v in triangles[face]]
        low = [min(p[c] for p in points) for c in range(3)]
        high = [max(p[c] for p in points) for c in range(3)]
        volume = 0.0
        for face in face_ids:
            a, b, c = [vertices[v] for v in triangles[face]]
            volume += (a[0]*(b[1]*c[2]-b[2]*c[1]) + a[1]*(b[2]*c[0]-b[0]*c[2]) + a[2]*(b[0]*c[1]-b[1]*c[0])) / 6
        assert volume > 0, 'Inverted or empty shell'
        assert abs(low[2]) < 1e-5, 'Part is not on the print bed'
        components.append({'min': low, 'max': high, 'volumeMM3': volume})
    assert len(components) == item['shells']
    for i, a in enumerate(components):
        for b in components[i + 1:]:
            separation = max(max(a['min'][c]-b['max'][c], b['min'][c]-a['max'][c]) for c in (0, 1))
            assert separation >= 3.999, 'Print parts too close or overlapping'
    dimensions = [max(p[c] for p in vertices)-min(p[c] for p in vertices) for c in range(3)]
    assert all(abs(a-b) < 1e-4 for a, b in zip(dimensions, item['dimensionsMM']))
    reports.append({'file': item['filename'], 'passed': True, 'watertight': True,
                    'shells': len(components), 'dimensionsMM': dimensions,
                    'sha256': hashlib.sha256(raw).hexdigest(), 'components': components})
output = Path(__file__).with_name('print-stl-results.json')
output.write_text(json.dumps(reports, indent=2) + '\n')
print(json.dumps([{k: v for k, v in r.items() if k != 'components'} for r in reports], indent=2))
