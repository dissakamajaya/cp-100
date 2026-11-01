# Four-pad horizontal controller — 30 October 2026

User selected **1×4 horizontal row**, superseding the initial 2×2 assumption. Updated model, STL exports, ZIP, preview and project handoff/decisions. Previous 2×2 evidence remains historical in evidence/enclosure-4-pad-2026-10-30/.

Envelope: **220×74×24 mm**; 40×40 mm pad faces at 48 mm pitch; 41×41/R2 windows. Custom 198×52 mm PCB is a fit reference only. Print one top, one base and four copies of the single cap mesh. Eight-pad STL archive preserved. Service controller geometry unchanged.

## Geometry — PASS

```sh
node enclosure/export-stl.mjs
node evidence/enclosure-4-pad-row-2026-10-30/verify-geometry.mjs
```

Artifact: geometry-results.json. Seven printable part types are valid single solids with positive volume. STL byte counts and print-bed origins checked. Four cap centres share one Y coordinate, with X = 38, 86, 134, 182 mm. No cap/housing/neighbour positive-volume intersections. Footprint 220×74 verified. Legacy eight-pad generation and unchanged service geometry checked.

## Stagehand preview — PASS

Server: node enclosure/serve.mjs 5181.

```sh
stagehand-run /Users/0xtx/Documents/GitHub/cp-100/evidence/enclosure-4-pad-row-2026-10-30/verify-preview.mjs
```

Returned 7 printed parts, fourPadLabels=true, layout=1 row x 4 pads. Artifacts: pad-front.png, pad-assembled.png, pad-exploded.png, pad-underside.png. Assembled image visually inspected: four pads in one horizontal row.

## Repository and delivery — PASS

Before edits, main was ahead of origin by one commit with pre-existing dirty/untracked work, preserved.

```sh
node --test simulator/tests/*.test.js domain-router/worker.test.mjs
python3 scripts/verify_manifest.py
```

44/44 tests passed before editing; 86 packaged assets matched the manifest, also rechecked after edits. ZIP integrity and included README content checked; SHA-256 hashes in delivery-manifest.json. git diff --check passed.

No physical print/hardware verification. Firmware, simulator and PDF remain eight-pad. No commit, push or deployment.

## Ready-to-slice STL delivery — PASS

User requested a ready-to-print STL. Created `enclosure/print-ready/CP-100-4-pad-horizontal-PRINT.stl`: six separated shells (one top, one base, four caps), all at Z=0, footprint 220×208 mm. Housing-only and four-cap layouts support separate materials. No source mechanical dimensions changed.

Commands: `node enclosure/export-print-plate.mjs`; `python3 evidence/enclosure-4-pad-row-2026-10-30/verify-print-stl.py`. Artifact: `print-stl-results.json`. Checks on actual float32 STL exports: closed manifold edges, consistent winding, positive-volume components, required shell counts, ground contact, footprint and at least 4 mm part spacing. All pass. ZIP rebuilt and all contents byte-compared with current files; hashes updated in `delivery-manifest.json`.

Before this edit: 44 simulator/router tests and 86-asset manifest passed. No physical printing or printer-specific slicing performed.

## Vendor PNG — PASS

Artifact: `enclosure/print-ready/CP-100-4-pad-vendor-preview.png` (2400×1800). Rendered directly from the delivered STL with Three.js STLLoader via `enclosure/vendor-preview.html`. Six print parts, footprint and assembled dimensions shown. Stagehand returned 7,884 triangles and 220×208×21 mm bounds; image visually inspected for geometry, labels and clipping. Image/STL hashes: `vendor-preview-results.json`.

Command: `stagehand-run /Users/0xtx/Documents/GitHub/cp-100/evidence/enclosure-4-pad-row-2026-10-30/render-vendor-preview.mjs`; server: `node enclosure/serve.mjs 5181`. Initial attempts failed because the previous local server had stopped; restarting the server resolved the render.
