import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import Module from '../../enclosure/node_modules/manifold-3d/manifold.js';
import { build, placed, printable, PARAMS } from '../../enclosure/model.mjs';

const wasm = await Module();
wasm.setup();
const parts = build(wasm);
const top = parts.find(p => p.id === 'pad-top');
const base = parts.find(p => p.id === 'pad-base');
const cap = parts.find(p => p.id === 'pad-cap');
assert.equal(cap.count, 4);
assert.equal(cap.instances.length, 4);
const checks = [];
for (const part of parts.filter(p => !p.ref)) {
  assert.equal(part.manifold.status(), 'NoError');
  assert.equal(part.manifold.decompose().length, 1);
  assert.ok(part.manifold.volume() > 0);
  const printed = printable(part);
  const { min, max } = printed.boundingBox();
  assert.ok(Math.abs(min[2]) < 1e-6);
  const dimensions = max.map((v, i) => v - min[i]);
  const file = `CP-100-${part.id}${part.count > 1 ? `-x${part.count}` : ''}.stl`;
  const bytes = readFileSync(new URL(`../../enclosure/stl/${file}`, import.meta.url));
  assert.equal(bytes.length, 84 + bytes.readUInt32LE(80) * 50);
  checks.push({ file, dimensions, count: part.count, status: part.manifold.status(), solids: 1 });
}
for (const part of [top, base]) {
  const { min, max } = part.manifold.boundingBox();
  assert.ok(Math.abs(max[0] - min[0] - 124) < 1e-6);
  assert.ok(Math.abs(max[1] - min[1] - 122) < 1e-6);
}
const caps = placed(cap);
for (let i = 0; i < caps.length; i++) {
  for (const other of [top.manifold, base.manifold, ...caps.slice(i + 1)]) {
    assert.ok(Math.abs(caps[i].intersect(other).volume()) < 1e-6, 'Pad caps must not intersect housing or neighbours');
  }
}
const legacy = build(wasm, { ...PARAMS, padColumns: 4 });
assert.equal(legacy.find(p => p.id === 'pad-cap').count, 8);
for (const part of parts.filter(p => p.group === 'Service controller')) {
  const previous = legacy.find(p => p.id === part.id).manifold;
  assert.ok(Math.abs(part.manifold.subtract(previous).volume()) < 1e-6);
  assert.ok(Math.abs(previous.subtract(part.manifold).volume()) < 1e-6);
}
const result = { passed: true, checks, capsWithoutCollisions: caps.length, legacyEightPadAvailable: true, serviceGeometryUnchanged: true };
writeFileSync(new URL('./geometry-results.json', import.meta.url), JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify(result, null, 2));
