export const SEQUENCE_INTERVAL_MS = 3000;

export class PadSequencer {
  constructor({ onStep, random = Math.random, schedule = (callback, delay) => setTimeout(callback, delay), cancel = timer => clearTimeout(timer) }) {
    this.onStep = onStep; this.random = random; this.schedule = schedule; this.cancel = cancel;
    this.running = false; this.mode = 'ordered'; this.index = null; this.queue = []; this.timer = null; this.generation = 0;
  }
  start(mode = 'ordered') {
    if (!['ordered', 'random'].includes(mode)) throw new Error('Urutan sequence tidak valid.');
    this.stop(); this.mode = mode; this.running = true;
    this.advance(this.generation);
  }
  stop() {
    this.generation++; this.running = false; this.index = null; this.queue = [];
    if (this.timer !== null) this.cancel(this.timer);
    this.timer = null;
  }
  advance(generation) {
    if (!this.running || generation !== this.generation) return;
    if (!this.queue.length) {
      this.queue = Array.from({ length: 8 }, (_, i) => i);
      if (this.mode === 'random') {
        for (let i = 7; i > 0; i--) {
          const j = Math.floor(this.random() * (i + 1));
          [this.queue[i], this.queue[j]] = [this.queue[j], this.queue[i]];
        }
        // Keep the last pad of one shuffled cycle from repeating immediately.
        if (this.queue[0] === this.index) [this.queue[0], this.queue[1]] = [this.queue[1], this.queue[0]];
      }
    }
    this.index = this.queue.shift(); this.onStep(this.index);
    if (this.running && generation === this.generation) {
      this.timer = this.schedule(() => this.advance(generation), SEQUENCE_INTERVAL_MS);
    }
  }
  diagnostics() { return { running: this.running, mode: this.mode, index: this.index, intervalMs: SEQUENCE_INTERVAL_MS }; }
}
