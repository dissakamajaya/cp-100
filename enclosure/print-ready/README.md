# CP-100 — four pads in one horizontal row

Import **CP-100-4-pad-horizontal-PRINT.stl** into your slicer at **100% scale, millimetres**. It contains six separate parts already oriented on Z=0: one top panel, one base and four pad caps. This is a print layout, not an assembled or print-in-place object.

The layout occupies **220×208 mm**, with maximum height 21 mm. Allow extra bed space for the slicer's brim/skirt and printer exclusions; a 256×256 mm usable bed provides margin. Select your printer and material profile before slicing. Review supports/bridging at the rear cable opening. STL does not contain printer-specific temperatures or toolpaths.

For different materials/colours:

- **CP-100-housing-PRINT.stl**: top and base, footprint 220×156 mm. Use this for the black housing.
- **CP-100-four-caps-PRINT.stl**: all four caps, footprint 188×44 mm. Use this for translucent pads; no duplication is needed.

Assembled enclosure: 220×74×24 mm. Pad faces: 40×40 mm, 48 mm centre spacing. Caps have 44×44 mm retention flanges. Prototype geometry requires a physical fit/travel test with the actual switches and PCB.

Regenerate from the repository root with `node enclosure/export-print-plate.mjs`. Validate exported STL topology with `python3 evidence/enclosure-4-pad-row-2026-10-30/verify-print-stl.py`. These checks cover closed, consistently wound meshes, positive-volume shells, spacing and every part resting on the bed; they do not constitute a physical print test.
