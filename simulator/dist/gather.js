// GATHER: one-shot routine that glides between sine steps to herd sand back to the
// middle of the glass. Frequencies must be found on the real plate; defaults are examples.
export const GATHER_KEY = 'chladni-08-gather-v1';
export const GATHER_LIMITS = Object.freeze({ steps: [1, 16], frequencyHz: [20, 4000], levelPct: [0, 100], glideS: [0, 60], holdS: [0, 60], totalS: 300 });
export const GATHER_FADE_S = 2;
export const GATHER_FRAME_MS = 40;
export function exampleGather() {
  return [
    { frequencyHz: 55.6, levelPct: 40, glideS: 2, holdS: 4 },
    { frequencyHz: 88.2, levelPct: 36, glideS: 4, holdS: 4 },
    { frequencyHz: 140, levelPct: 32, glideS: 4, holdS: 4 },
    { frequencyHz: 222.2, levelPct: 26, glideS: 4, holdS: 4 },
    { frequencyHz: 352.8, levelPct: 20, glideS: 4, holdS: 4 },
  ];
}
const inside = (value, [min, max]) => typeof value === 'number' && Number.isFinite(value) && value >= min && value <= max;
export function gatherDuration(steps) { return steps.reduce((sum, s) => sum + s.glideS + s.holdS, 0) + GATHER_FADE_S; }
export function cleanGather(steps) {
  const [minSteps, maxSteps] = GATHER_LIMITS.steps;
  if (!Array.isArray(steps) || steps.length < minSteps || steps.length > maxSteps) throw new Error(`GATHER perlu ${minSteps}-${maxSteps} langkah.`);
  const clean = steps.map((s, i) => {
    if (!s || !['frequencyHz', 'levelPct', 'glideS', 'holdS'].every(key => inside(s[key], GATHER_LIMITS[key]))) throw new Error(`Langkah ${i + 1} GATHER tidak valid.`);
    return { frequencyHz: Math.round(s.frequencyHz * 10) / 10, levelPct: Math.round(s.levelPct), glideS: Math.round(s.glideS * 10) / 10, holdS: Math.round(s.holdS * 10) / 10 };
  });
  if (gatherDuration(clean) > GATHER_LIMITS.totalS) throw new Error(`GATHER maksimal ${GATHER_LIMITS.totalS} detik.`);
  return clean;
}
export function exportGatherFile(steps) { return { schema: 'cp-100-gather', version: 1, steps: cleanGather(steps) }; }
export function parseGatherFile(value) {
  const object = typeof value === 'string' ? JSON.parse(value) : value;
  if (!object || object.schema !== 'cp-100-gather' || object.version !== 1) throw new Error('File GATHER tidak dikenal.');
  return cleanGather(object.steps);
}
// Output at time t (seconds). The first glide fades in from silence; frequency glides
// exponentially so each octave takes equal time. Returns null once the final fade ends.
export function gatherAt(steps, t) {
  let start = 0;
  for (let i = 0; i < steps.length; i++) {
    const step = steps[i], previous = steps[i - 1];
    if (t < start + step.glideS) {
      const u = (t - start) / step.glideS;
      if (!previous) return { index: i, phase: 'glide', frequencyHz: step.frequencyHz, levelPct: step.levelPct * u };
      return { index: i, phase: 'glide', frequencyHz: previous.frequencyHz * (step.frequencyHz / previous.frequencyHz) ** u, levelPct: previous.levelPct + (step.levelPct - previous.levelPct) * u };
    }
    start += step.glideS;
    if (t < start + step.holdS) return { index: i, phase: 'hold', frequencyHz: step.frequencyHz, levelPct: step.levelPct };
    start += step.holdS;
  }
  const last = steps[steps.length - 1];
  if (t < start + GATHER_FADE_S) return { index: steps.length - 1, phase: 'fade', frequencyHz: last.frequencyHz, levelPct: last.levelPct * (1 - (t - start) / GATHER_FADE_S) };
  return null;
}
export class GatherPlayer {
  constructor({ onFrame, onDone = () => {}, now = () => performance.now(), schedule = (callback, delay) => setTimeout(callback, delay), cancel = timer => clearTimeout(timer) }) {
    this.onFrame = onFrame; this.onDone = onDone; this.now = now; this.schedule = schedule; this.cancel = cancel;
    this.running = false; this.steps = []; this.frame = null; this.timer = null; this.generation = 0; this.startedAt = 0;
  }
  start(steps) {
    const clean = cleanGather(steps);
    this.stop(); this.steps = clean; this.running = true; this.startedAt = this.now();
    this.tick(this.generation);
  }
  stop() {
    this.generation++; this.running = false; this.frame = null;
    if (this.timer !== null) this.cancel(this.timer);
    this.timer = null;
  }
  tick(generation) {
    if (!this.running || generation !== this.generation) return;
    this.frame = gatherAt(this.steps, (this.now() - this.startedAt) / 1000);
    if (!this.frame) { this.stop(); this.onDone(); return; }
    this.onFrame(this.frame);
    if (this.running && generation === this.generation) this.timer = this.schedule(() => this.tick(generation), GATHER_FRAME_MS);
  }
  diagnostics() { return { running: this.running, frame: this.frame && { ...this.frame }, durationS: this.steps.length ? gatherDuration(this.steps) : 0 }; }
}
