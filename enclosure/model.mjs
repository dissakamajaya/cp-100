// CP-100 printable enclosures; four-pad revision requested 30 October 2026.
// Pad pitch/caps retain the geometry from the Rev08 cutout table
// (outputs/CP-100-Rev08-Blueprint/cutout-coordinates-mm.csv).
// Units mm. Blueprint datum is top-left with Y down; model uses Z up and
// y = panelHeight - blueprintY so the front view matches the drawings.
// Shared by the browser preview (index.html) and export-stl.mjs.

export const PARAMS = {
  padColumns: 4,   // four pads in one horizontal row
  padRows: 1,
  plate: 3,        // printed panel thickness (blueprint aluminium is 2)
  wall: 2.4,
  floor: 2,
  pilot: 2.5,      // M3 self-tapping pilot
  m3: 3.4,
  padBoxHeight: 24, // blueprint: pad box target <= 24
  svcDepth: 40,     // internal depth behind the service panel
  ffc: [27, 3],     // flat flex cable slot (25 mm cable + clearance)
};

export function build(wasm, p = PARAMS) {
  const { Manifold, CrossSection } = wasm;
  const SEG = 48;

  const rrect = (w, h, r) =>
    r > 0
      ? CrossSection.square([w - 2 * r, h - 2 * r]).translate([r, r]).offset(r, 'Round', 2, SEG)
      : CrossSection.square([w, h]);
  // Rounded box, corner at (x, y, z).
  const rbox = (x, y, z, w, h, d, r = 0) => rrect(w, h, r).extrude(d).translate([x, y, z]);
  const cyl = (x, y, z, d, h, seg = SEG) => Manifold.cylinder(h, d / 2, d / 2, seg).translate([x, y, z]);
  const union = (list) => Manifold.union(list);

  const parts = [];
  // Bought parts, shown in the preview for fit only; never exported.
  const ref = (group, id, name, color, list, note, explode = [0, 0, 0]) =>
    parts.push({ id, group, name, color, manifold: union(list), ref: true, explode, count: 1, note });

  // ---------- Pad controller (visitor panel 220 x 74, 1 row x 4 pads) ----------
  {
    const W = 28 + 48 * p.padColumns, H = 26 + 48 * p.padRows;
    const T = p.plate, top = p.padBoxHeight, under = top - T;
    const padCount = p.padColumns * p.padRows;
    const pcbW = W - 22, pcbH = H - 22;
    const fy = (y) => H - y; // blueprint y -> model y
    const windows = [];
    for (let row = 0; row < p.padRows; row++) for (let col = 0; col < p.padColumns; col++) {
      windows.push({ x: 17.5 + 48 * col, y: 16.5 + 48 * row });
    }
    const corners = [[7, 7], [W - 7, 7], [7, H - 7], [W - 7, H - 7]];
    const centers = windows.map((w) => [w.x + 20.5, fy(w.y + 20.5)]);

    let plate = rbox(0, 0, under, W, H, T, 3);
    // Guide sleeves for the 44 x 44 cap flange, 4 mm deep under the plate.
    const sleeves = centers.map(([cx, cy]) =>
      rbox(cx - 23.8, cy - 23.8, under - 4, 47.6, 47.6, 4).subtract(rbox(cx - 22.3, cy - 22.3, under - 4.1, 44.6, 44.6, 4.2)));
    plate = union([plate, ...sleeves]);
    plate = plate.subtract(union([
      ...windows.map((w) => rbox(w.x, fy(w.y + 41), under - 5, 41, 41, T + 6, 2)),
      ...corners.map(([x, y]) => cyl(x, fy(y), under - 1, p.m3, T + 2)),
    ]));
    parts.push({
      id: 'pad-top', group: 'Pad controller', name: 'Top panel', color: '#202226', manifold: plate,
      explode: [0, 0, 40], print: { rotate: [180, 0, 0] }, count: 1,
      note: `${W} x ${H} x ${T}, ${padCount} windows 41 x 41 R2 (${p.padRows} row x ${p.padColumns} pads), M3 corners. Print face down.`,
    });

    // Base tray: walls up to the plate, corner bosses, PCB standoffs, cable hole.
    let base = rbox(0, 0, 0, W, H, under, 3).subtract(rbox(p.wall, p.wall, p.floor, W - 2 * p.wall, H - 2 * p.wall, under, 1));
    const pcbBottom = 12 - 1.6; // blueprint: PCB top at Z = 12
    const standoffs = [[15, 15], [W - 15, 15], [15, H - 15], [W - 15, H - 15]];
    base = union([
      base,
      ...corners.map(([x, y]) => cyl(x, fy(y), 0, 7, under)),
      ...standoffs.map(([x, y]) => cyl(x, fy(y), 0, 6, pcbBottom)),
    ]);
    base = base.subtract(union([
      ...corners.map(([x, y]) => cyl(x, fy(y), under - 10, p.pilot, 10.1)),
      ...standoffs.map(([x, y]) => cyl(x, fy(y), p.floor, p.pilot, pcbBottom)),
      // Flex cable slot in the rear wall, below the PCB.
      rbox(W / 2 - p.ffc[0] / 2, H - 5, 6 - p.ffc[1] / 2, p.ffc[0], 10, p.ffc[1]),
    ]));
    parts.push({
      id: 'pad-base', group: 'Pad controller', name: 'Base tray', color: '#3a3d44', manifold: base,
      explode: [0, 0, 0], print: { rotate: [0, 0, 0] }, count: 1,
      note: `${W} x ${H} x ${under}. Standoffs for custom ${pcbW} x ${pcbH} PCB at X/Y = 11 (PCB top Z 12). 27 x 3 flex cable slot in the rear wall.`,
    });

    // Pad cap: 40 x 40 face flush with the plate, 44 x 44 flange stops under it,
    // D4 plunger to 0.1 mm above the B3F (4.3 tall on the PCB).
    const cap = union([
      rbox(-20, -20, under, 40, 40, T, 2),
      rbox(-22, -22, under - 1, 44, 44, 1, 2),
      cyl(0, 0, 12 + 4.3 + 0.1, 4, under - 1 - (12 + 4.3 + 0.1)),
    ]);
    parts.push({
      id: 'pad-cap', group: 'Pad controller', name: 'Pad cap', color: '#f1eee6', opacity: 0.85, manifold: cap,
      instances: centers.map(([x, y]) => [x, y, 0]), explode: [0, 0, 70], print: { rotate: [180, 0, 0] }, count: padCount,
      note: `Print ${padCount} in translucent/white PETG so the WS2812B shows through. Face down on the bed.`,
    });

    ref('Pad controller', 'ref-pad-pcb', 'Pad PCB + B3F + WS2812B', '#2f7d4f', [
      rbox(11, fy(H - 11), pcbBottom, pcbW, pcbH, 1.6),
      ...centers.map(([x, y]) => rbox(x - 3, y - 3, 12, 6, 6, 4.3)),
      ...centers.map(([x, y]) => rbox(x - 2.5, y + 12 - 2.5, 12, 5, 5, 1.6)),
    ], `Custom ${pcbW} x ${pcbH} PCB, tactile under each pad, RGB 12 mm off centre. Reference only; no PCB fabrication files.`);
    ref('Pad controller', 'ref-pad-ffc', 'Flex cable', '#d9a441', [
      rbox(W / 2 - 12.5, H - 17, 5.9, 25, 6 + 30, 0.3),
    ], '25 mm flat flex, exits the rear wall.');
  }

  // ---------- Service controller (compact 140 x 90, sized for the 0.96" OLED) ----------
  // Replaces the Rev08 260 x 180 layout. Top row: OLED + FREQ/LEVEL/FOLD at 28 mm
  // pitch (D22 knobs). Bottom row: WAVE, HARMONICS, MODE slides, SAVE, TEST/MUTE.
  // Controls hang under the panel; the perfboard sits below them on the back.
  {
    const W = 140, H = 90, T = p.plate, under = p.floor + p.svcDepth, top = under + T;
    const fy = (y) => H - y;
    const corners = [[6, 6], [134, 6], [6, 84], [134, 84]];
    const encoders = [[66, 24, 'FREQ'], [94, 24, 'LEVEL'], [122, 24, 'FOLD']];
    const buttons = [[100, 64, 'SAVE'], [124, 64, 'TEST/MUTE']];
    const slides = [[22, 64, 12.6, 10, 6, 'WAVE'], [48, 64, 12.6, 10, 6, 'HARMONICS'], [74, 64, 8.6, 6, 5, 'MODE']];

    // 0.96" I2C OLED (SSD1306 128 x 64, yellow/blue), about 27 x 27.8 PCB with
    // M2 holes 23.5 x 23.8 apart. Glass sits on the panel; 1.6 mm posts carry the
    // PCB, screwed from behind with M2 x 4. Measure the module before printing.
    const dispCx = 26, dispCy = fy(24);
    const oledGlass = 1.6;
    const oledHoles = [[-11.75, -11.9], [11.75, -11.9], [-11.75, 11.9], [11.75, 11.9]].map(([dx, dy]) => [dispCx + dx, dispCy + dy]);
    let panel = rbox(0, 0, under, W, H, T, 3);
    panel = union([
      panel,
      ...oledHoles.map(([x, y]) => cyl(x, y, under - oledGlass, 4.5, oledGlass)),
      ...buttons.map(([x, y]) => cyl(x, fy(y), under - 4, 18.6, 4).subtract(cyl(x, fy(y), under - 4.1, 15.6, 4.2))),
    ]);
    panel = panel.subtract(union([
      rbox(dispCx - 12.5, dispCy - 7.5, under - 1, 25, 15, T + 2, 1),
      ...slides.map(([x, y, , w, h]) => rbox(x - w / 2, fy(y) - h / 2, under - 1, w, h, T + 2, 1)),
      ...corners.map(([x, y]) => cyl(x, fy(y), under - 1, p.m3, T + 2)),
      ...encoders.map(([x, y]) => cyl(x, fy(y), under - 1, 7.2, T + 2)),
      ...buttons.map(([x, y]) => cyl(x, fy(y), under - 1, 12.6, T + 2)),
      ...oledHoles.map(([x, y]) => cyl(x, y, under - oledGlass - 0.1, 1.6, oledGlass + 2.3)),
    ]));
    parts.push({
      id: 'svc-panel', group: 'Service controller', name: 'Front panel', color: '#202226', manifold: panel,
      explode: [0, 0, 60], print: { rotate: [180, 0, 0] }, count: 1,
      note: '140 x 90 x 3. OLED window 25 x 15 with 4 M2 posts, encoders D7.2, slides 10 x 6, MODE 6 x 5, buttons D12.6. Print face down.',
    });

    // Box: back + walls up to the panel, corner bosses, 70 x 90 perfboard standoffs
    // (ESP32-S3 DevKitC-1, PCM5102, MCP23017, AHCT), USB-C slot, two cable holes.
    let box = rbox(0, 0, 0, W, H, under, 3).subtract(rbox(p.wall, p.wall, p.floor, W - 2 * p.wall, H - 2 * p.wall, under, 1));
    const board = { x: 12, y: 10, w: 90, h: 70 };
    const boardHoles = [[2.5, 2.5], [87.5, 2.5], [2.5, 67.5], [87.5, 67.5]].map(([dx, dy]) => [board.x + dx, fy(board.y + dy)]);
    const standoffH = 8;
    // DevKitC-1 on 8.5 mm female headers; USB-C sits about 1.6 mm above its PCB.
    const devkitZ = p.floor + standoffH + 1.6 + 8.5;
    const usbZ = devkitZ + 1.6 + 1.6;
    box = union([
      box,
      ...corners.map(([x, y]) => cyl(x, fy(y), 0, 8, under)),
      ...boardHoles.map(([x, y]) => cyl(x, y, 0, 6, p.floor + standoffH)),
    ]);
    box = box.subtract(union([
      ...corners.map(([x, y]) => cyl(x, fy(y), under - 12, p.pilot, 12.1)),
      ...boardHoles.map(([x, y]) => cyl(x, y, p.floor, p.pilot, standoffH + 0.1)),
      rbox(-1, fy(board.y + 35) - 7, usbZ - 5, 5, 14, 10, 1),
      // Rear wall: flex cable from the pad controller (just above the board) and D8 for amplifier wires.
      rbox(112 - p.ffc[0] / 2, H - 5, 14 - p.ffc[1] / 2, p.ffc[0], 10, p.ffc[1]),
      Manifold.cylinder(10, 4, 4, SEG).rotate([90, 0, 0]).translate([60, H + 5, 32]),
    ]));
    parts.push({
      id: 'svc-box', group: 'Service controller', name: 'Box', color: '#3a3d44', manifold: box,
      explode: [0, 0, 0], print: { rotate: [0, 0, 0] }, count: 1,
      note: `140 x 90 x ${under.toFixed(1)}. Standoffs for a 70 x 90 perfboard, USB-C slot on left wall; 27 x 3 flex slot and D8 amplifier hole in the rear wall.`,
    });

    // Knob D22 x 16, D-shaft 6 (flat at 4.5), pointer groove.
    const knobH = 16;
    let knob = cyl(0, 0, 0, 22, knobH, 64);
    const ridges = [];
    for (let i = 0; i < 30; i++) {
      const a = (i / 30) * 360;
      ridges.push(Manifold.cylinder(knobH - 2, 0.7, 0.7, 12).translate([11, 0, 1]).rotate([0, 0, a]));
    }
    knob = knob.subtract(union([
      ...ridges,
      cyl(0, 0, -0.1, 6.15, 12.1).subtract(rbox(1.5, -4, -1, 4, 8, 14)),
      rbox(-0.5, 3, knobH - 0.6, 1, 7, 1),
    ]));
    knob = knob.translate([0, 0, top + 1]);
    parts.push({
      id: 'svc-knob', group: 'Service controller', name: 'Knob', color: '#c9ccd3', manifold: knob,
      instances: encoders.map(([x, y]) => [x, fy(y), 0]), explode: [0, 0, 100], print: { rotate: [180, 0, 0] }, count: 3,
      note: 'D22 x 16, bore D6 with flat at 4.5 (PEC11R D-shaft). Measure the shaft before printing all three.',
    });

    // Button cap D12 through the D12.6 hole, D15 flange under the panel inside the guide.
    const btn = union([
      cyl(0, 0, under, 12, T + 1.5),
      cyl(0, 0, under - 1, 15, 1),
      cyl(0, 0, under - 8, 4, 7),
    ]);
    parts.push({
      id: 'svc-button', group: 'Service controller', name: 'Button cap', color: '#d94f6a', manifold: btn,
      instances: buttons.map(([x, y]) => [x, fy(y), 0]), explode: [0, 0, 85], print: { rotate: [180, 0, 0] }, count: 2,
      note: 'SAVE + TEST/MUTE. 1.5 mm proud of the panel. Plunger length depends on the B3F carrier height; trim to fit.',
    });

    ref('Service controller', 'ref-svc-controls', 'Encoders, slides, tactiles', '#8a6d3b', [
      ...encoders.flatMap(([x, y]) => [
        rbox(x - 6.2, fy(y) - 6.7, under - 6.5, 12.4, 13.4, 6.5),
        cyl(x, fy(y), under, 7, T + 2),
        cyl(x, fy(y), under, 6, T + 13),
        rbox(x - 12, fy(y) - 12, under - 8.1, 24, 24, 1.6),
      ]),
      ...slides.flatMap(([x, y, len]) => [
        rbox(x - len / 2, fy(y) - 2.2, under - 4, len, 4.4, 4),
        rbox(x - 1, fy(y) - 1, under, 2, 2, T + 1),
        rbox(x - 10, fy(y) - 10, under - 5.6, 20, 20, 1.6),
      ]),
      ...buttons.flatMap(([x, y]) => [
        rbox(x - 3, fy(y) - 3, under - 8.1 - 4.3, 6, 6, 4.3),
        rbox(x - 10, fy(y) - 10, under - 8.1 - 4.3 - 1.6, 20, 20, 1.6),
      ]),
    ], 'PEC11R x3, slide switches WAVE / HARMONICS / MODE, B3F for SAVE and TEST/MUTE, each on a small carrier PCB.', [0, 0, 60]);
    ref('Service controller', 'ref-svc-oled', 'OLED 0.96" I2C', '#3a7bd5', [
      rbox(dispCx - 13.35, dispCy - 6, under - oledGlass, 26.7, 19.3, oledGlass).translate([0, -3.6, 0]),
      rbox(dispCx - 13.5, dispCy - 13.9, under - oledGlass - 1.2, 27, 27.8, 1.2),
      rbox(dispCx - 5, dispCy + 11.5, under - oledGlass - 1.2 - 8.5, 10, 2.5, 8.5),
    ], 'SSD1306 128 x 64 yellow/blue, 4-pin I2C header (GND, VCC, SCL, SDA).', [0, 0, 60]);
    ref('Service controller', 'ref-svc-boards', 'Perfboard, ESP32-S3, PCM5102', '#2f7d4f', [
      rbox(board.x, fy(board.y + board.h), p.floor + standoffH, board.w, board.h, 1.6),
      rbox(board.x + 3, fy(board.y + 35) - 13.3, devkitZ, 69, 26.6, 1.6),
      rbox(board.x + 3, fy(board.y + 35) - 4.5, devkitZ + 1.6, 9, 9, 3.2),
      rbox(board.x + 40, fy(board.y + 35) - 9, devkitZ + 1.6, 25.5, 18, 3.2),
      rbox(board.x + 55, fy(board.y + 10) - 10, p.floor + standoffH + 1.6 + 2.5, 32, 20, 1.6),
    ], 'ESP32-S3 DevKitC-1 on headers (USB-C toward the left slot), GY-PCM5102, plus MCP23017 and AHCT on the same board.');
    ref('Service controller', 'ref-svc-ffc', 'Flex cable', '#d9a441', [
      rbox(112 - 12.5, H - 15, 13.85, 25, 45, 0.3),
    ], '25 mm flat flex from the pad controller, through the rear wall.');
  }

  return parts;
}

// Placed copies for the assembled view.
export function placed(part) {
  return (part.instances ?? [[0, 0, 0]]).map((t) => part.manifold.translate(t));
}

// Single copy, rotated for printing and dropped onto the bed at the origin.
export function printable(part) {
  const m = part.manifold.rotate(part.print.rotate);
  const { min, max } = m.boundingBox();
  return m.translate([-(min[0] + max[0]) / 2, -(min[1] + max[1]) / 2, -min[2]]);
}
