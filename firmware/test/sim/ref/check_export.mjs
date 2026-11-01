// Feeds the firmware's "export" output to the simulator's own importer.
// node check_export.mjs <file>   -> prints one CHECK line
import { readFileSync } from 'node:fs';
import { parsePresetFile } from './model.mjs';
const name = 'console/export_imports_in_simulator';
try {
  const text = readFileSync(process.argv[2], 'utf8').trim();
  const presets = parsePresetFile(text);
  const used = presets.map((p, i) => p ? `${i + 1}:${p.frequencyHz}Hz/${p.wave}/${p.harmonics}` : null).filter(Boolean);
  const ok = presets.length === 8 && presets[5] && presets[5].frequencyHz === 220 && presets[5].wave === 'triangle';
  console.log(`CHECK ${ok ? 'PASS' : 'FAIL'} ${name}: parsePresetFile accepted ${used.length} presets (${used.join(' ')})`);
} catch (e) {
  console.log(`CHECK FAIL ${name}: ${e.message}`);
}
