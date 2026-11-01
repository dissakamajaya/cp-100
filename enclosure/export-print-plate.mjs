// Four-pad horizontal controller, separated into print-oriented shells.
import assert from 'node:assert/strict';
import { mkdirSync, writeFileSync } from 'node:fs';
import Module from 'manifold-3d';
import { build, printable } from './model.mjs';

const wasm = await Module();
wasm.setup();
const parts = build(wasm);
const part = id => parts.find(p => p.id === id);
assert.equal(part('pad-cap').count, 4);
const top = printable(part('pad-top'));
const base = printable(part('pad-base'));
const cap = printable(part('pad-cap'));
const housing = [top.translate([110, 37, 0]), base.translate([110, 119, 0])];
const caps = [22, 70, 118, 166].map(x => cap.translate([x, 22, 0]));
const all = [...housing, ...caps.map(m => m.translate([16, 164, 0]))];
const dir = new URL('./print-ready/', import.meta.url);
mkdirSync(dir, { recursive: true });

function exportPlate(filename, shells) {
  for (const shell of shells) {
    assert.equal(shell.status(), 'NoError');
    assert.equal(shell.decompose().length, 1);
    assert.ok(Math.abs(shell.boundingBox().min[2]) < 1e-6);
  }
  const plate = wasm.Manifold.union(shells);
  assert.equal(plate.decompose().length, shells.length);
  const { vertProperties: v, triVerts: t, numProp: n } = plate.getMesh();
  const buf = Buffer.alloc(84 + (t.length / 3) * 50);
  buf.write('CP-100 four-pad horizontal controller; millimetres; separate print parts', 0, 'ascii');
  buf.writeUInt32LE(t.length / 3, 80);
  for (let i = 0; i < t.length / 3; i++) {
    const points = [0, 1, 2].map(k => [0, 1, 2].map(c => v[t[i * 3 + k] * n + c]));
    const u = points[1].map((x, c) => x - points[0][c]);
    const w = points[2].map((x, c) => x - points[0][c]);
    const normal = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]];
    const length = Math.hypot(...normal);
    assert.ok(length > 0);
    [...normal.map(x => x / length), ...points.flat()].forEach((x, j) => buf.writeFloatLE(x, 84 + i * 50 + j * 4));
  }
  writeFileSync(new URL(filename, dir), buf);
  const { min, max } = plate.boundingBox();
  return { filename, shells: shells.length, dimensionsMM: max.map((v, i) => v - min[i]), triangles: t.length / 3 };
}

const manifest = [
  exportPlate('CP-100-4-pad-horizontal-PRINT.stl', all),
  exportPlate('CP-100-housing-PRINT.stl', housing),
  exportPlate('CP-100-four-caps-PRINT.stl', caps),
];
writeFileSync(new URL('manifest.json', dir), JSON.stringify(manifest, null, 2) + '\n');
console.log(JSON.stringify(manifest, null, 2));
