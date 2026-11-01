# CP-100 controller 3D models

The visitor pad controller now has **four pads in a 1×4 horizontal layout**, per the request of 30 October 2026. Its assembled envelope is **220×74×24 mm** (W×D×H), with 40×40 mm pad faces, 41×41 mm rounded windows and 48 mm centre spacing. The top skin is 3 mm thick; the guide sleeves make the exported top part 7 mm tall.

## Four-pad print files

**Ready-to-slice layout:** `print-ready/CP-100-4-pad-horizontal-PRINT.stl` contains one top, one base and all four caps, laid flat and separated on a 220×208 mm footprint. Separate housing and four-cap plate files are provided for different colours/materials. See `print-ready/README.md`. Regenerate with `node export-print-plate.mjs`.

- `stl/CP-100-pad-top.stl`: one top panel, face down.
- `stl/CP-100-pad-base.stl`: one base tray, floor down.
- `stl/CP-100-pad-cap-x4.stl`: **one cap mesh; print four copies**, face down. Translucent/white PETG is the suggested diffuser material.
- `CP-100-4-pad-STLs.zip`: the individual STL files, ready-to-slice layouts and print instructions.

STL coordinates use millimetres. Files are already oriented with the lowest point at Z=0. The pad flange is 44×44 mm. The housing uses M3 corner clearance holes and 2.5 mm pilot holes. The rear cable slot remains 27×3 mm.

The displayed 198×52×1.6 mm PCB, B3F switches, WS2812B LEDs and flex cable are fit references, not fabrication files. Actual components, screw lengths, print clearance, return action and switch travel need a physical prototype. Print one cap before the full set. This is prototype CAD, not a hardware-validated production design.

## Preview and regeneration

```sh
cd /Users/0xtx/Documents/GitHub/cp-100/enclosure
npm ci
npm run serve
npm run stl
```

Open `http://127.0.0.1:5180/`. The preview supports assembly/exploded views, underside, component references and X-ray. `model.mjs` is the editable parametric source shared with the STL exporter.

The previous eight-pad STLs are preserved in `stl/legacy-8-pad/`. `build(wasm, { ...PARAMS, padColumns: 4, padRows: 2 })` reproduces the previous eight-pad layout. The separate service enclosure geometry is unchanged.

The simulator, firmware and Rev08 PDF still describe eight pads. Converting their pad counts, preset storage, LEDs, keyboard mappings, sequence and wiring is a separate follow-up; these STL files do not update the electronics or published site.
