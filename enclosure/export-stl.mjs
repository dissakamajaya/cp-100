// Writes one binary STL per part (single copy, print orientation) to stl/.
import { mkdirSync, writeFileSync } from 'node:fs';
import Module from 'manifold-3d';
import { build, printable } from './model.mjs';

const wasm = await Module();
wasm.setup();
mkdirSync(new URL('./stl/', import.meta.url), { recursive: true });

for (const part of build(wasm).filter((p) => !p.ref)) {
  const mesh = printable(part).getMesh();
  const { vertProperties: v, triVerts: t, numProp: n } = mesh;
  const tris = t.length / 3;
  const buf = Buffer.alloc(84 + tris * 50);
  buf.writeUInt32LE(tris, 80);
  for (let i = 0; i < tris; i++) {
    const o = 84 + i * 50;
    const p = [0, 1, 2].map((k) => [0, 1, 2].map((c) => v[t[i * 3 + k] * n + c]));
    const u = p[1].map((x, c) => x - p[0][c]), w = p[2].map((x, c) => x - p[0][c]);
    const nx = u[1] * w[2] - u[2] * w[1], ny = u[2] * w[0] - u[0] * w[2], nz = u[0] * w[1] - u[1] * w[0];
    const len = Math.hypot(nx, ny, nz) || 1;
    [nx / len, ny / len, nz / len, ...p.flat()].forEach((x, j) => buf.writeFloatLE(x, o + j * 4));
  }
  const file = `CP-100-${part.id}${part.count > 1 ? `-x${part.count}` : ''}.stl`;
  writeFileSync(new URL(`./stl/${file}`, import.meta.url), buf);
  const { min, max } = printable(part).boundingBox();
  console.log(file, max.map((x, c) => (x - min[c]).toFixed(1)).join(' x '), `${tris} tris`, `genus ${part.manifold.genus()}`);
}
