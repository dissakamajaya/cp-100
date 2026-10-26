export function oscillatorSample(phase, wave) {
  const t = ((phase % 1) + 1) % 1;
  if (wave === 'sawtooth') return 2 * t - 1;
  if (wave === 'triangle') return 1 - 4 * Math.abs(t - 0.5);
  return Math.sin(2 * Math.PI * t);
}
export function reflect(x) {
  const wrapped = ((x + 1) % 4 + 4) % 4;
  return wrapped <= 2 ? wrapped - 1 : 3 - wrapped;
}
// EVEN retains the source and adds only the even part of the biased transfer.
// ODD uses symmetric folding; BOTH sums that with the even part.
export function foldSample(x, foldPct, harmonics = 'odd') {
  if (foldPct <= 0) return x;
  const amount = foldPct / 100, driven = x * (1 + 8 * amount), bias = .55 * amount;
  const odd = reflect(driven);
  const even = (reflect(driven + bias) + reflect(-driven + bias)) * .5;
  return harmonics === 'even' ? x + even : harmonics === 'both' ? odd + even : odd;
}
const calibration = new Map();
export function calibrationFor(wave, foldPct, harmonics = 'odd') {
  const key = `${wave}:${foldPct}:${harmonics}`;
  if (calibration.has(key)) return calibration.get(key);
  let sum = 0, squared = 0;
  const count = 4096;
  for (let i = 0; i < count; i++) {
    const x = foldSample(oscillatorSample(i / count, wave), foldPct, harmonics);
    sum += x; squared += x * x;
  }
  const mean = sum / count;
  const value = { mean, gain: .36 / Math.sqrt(Math.max(1e-12, squared / count - mean * mean)) };
  if (calibration.size > 512) calibration.clear();
  calibration.set(key, value); return value;
}
export function period(parameters, count = 512) {
  const { wave, foldPct, harmonics, levelPct } = parameters;
  const { mean, gain } = calibrationFor(wave, foldPct, harmonics);
  return Float32Array.from({ length: count }, (_, i) => (foldSample(oscillatorSample(i / count, wave), foldPct, harmonics) - mean) * gain * levelPct / 100);
}
// Build this fixed curve once. All knob changes use AudioParam automation;
// never replace a curve or silence/restart the oscillator while sweeping.
let fixedCurve;
export function reflectionCurve() {
  return fixedCurve ||= Float32Array.from({length:16385},(_,i)=>reflect((2*i/16384-1)*16));
}
export class AudioPreview {
  constructor() { this.enabled = false; this.targets = new WeakMap(); }
  async enable() {
    const Context = globalThis.AudioContext || globalThis.webkitAudioContext;
    if (!Context) throw new Error('Browser ini belum mendukung audio simulasi.');
    if (!this.context) this.build(new Context());
    await this.context.resume();
    if (this.context.state !== 'running') throw new Error('Audio belum aktif. Tekan Audio sekali lagi.');
    this.enabled = true;
  }
  build(context) {
    this.context=context; this.source=context.createGain();this.sum=context.createGain();
    this.oscillators=['sine','sawtooth','triangle'].map(type=>{
      const oscillator=context.createOscillator(),gain=context.createGain();oscillator.type=type;
      gain.gain.value=type==='sine'?1:0;oscillator.connect(gain).connect(this.source);
      return {type,oscillator,gain};
    });
    const makeBranch=(initial)=>{
      const drive=context.createGain(),folder=context.createWaveShaper(),mix=context.createGain();
      drive.gain.value=initial;folder.curve=reflectionCurve();folder.oversample='4x';mix.gain.value=0;
      this.source.connect(drive).connect(folder).connect(mix).connect(this.sum);
      return {drive,folder,mix};
    };
    this.odd=makeBranch(1/16);this.plus=makeBranch(1/16);this.minus=makeBranch(-1/16);
    // Keep the dry path at the same oversampling latency as the folder paths.
    this.dry=makeBranch(1/16);
    this.bias=context.createConstantSource();this.bias.offset.value=0;
    this.bias.connect(this.plus.folder);this.bias.connect(this.minus.folder);this.bias.start();
    this.dcOffset=context.createConstantSource();this.dcOffset.offset.value=0;this.dcOffset.connect(this.sum);this.dcOffset.start();
    this.dcBlock=context.createBiquadFilter();this.dcBlock.type='highpass';this.dcBlock.frequency.value=5;this.dcBlock.Q.value=.707;
    this.gain=context.createGain();this.gain.gain.value=0;
    this.analyser=context.createAnalyser();this.analyser.fftSize=2048;
    this.sum.connect(this.dcBlock).connect(this.gain).connect(this.analyser).connect(context.destination);
    const start=context.currentTime+.01;this.oscillators.forEach(({oscillator})=>oscillator.start(start));
  }
  smooth(parameter,target,seconds=.02) {
    if(this.targets.get(parameter)===target)return;
    this.targets.set(parameter,target);
    const now=this.context.currentTime;
    parameter.cancelAndHoldAtTime(now);parameter.setTargetAtTime(target,now,seconds);
  }
  disable() { this.enabled=false;if(this.context)this.smooth(this.gain.gain,0,.01); }
  update(state) {
    if(!this.context)return;
    const p=state.parameters, amount=p.foldPct/100, driven=(1+8*amount)/16;
    const {mean,gain}=calibrationFor(p.wave,p.foldPct,p.harmonics);
    this.oscillators.forEach(({type,oscillator,gain:waveGain})=>{
      this.smooth(oscillator.frequency,p.frequencyHz,.015);
      this.smooth(waveGain.gain,type===p.wave?1:0,.012);
    });
    this.smooth(this.odd.drive.gain,driven);this.smooth(this.plus.drive.gain,driven);this.smooth(this.minus.drive.gain,-driven);
    this.smooth(this.bias.offset,.55*amount/16);
    this.smooth(this.odd.mix.gain,p.harmonics==='even'?0:1);
    this.smooth(this.dry.mix.gain,p.harmonics==='even'?1:0);
    this.smooth(this.plus.mix.gain,p.harmonics==='odd'?0:.5);
    this.smooth(this.minus.mix.gain,p.harmonics==='odd'?0:.5);
    this.smooth(this.dcOffset.offset,-mean);
    // Use the calibrated signal level directly; no extra browser attenuation.
    this.smooth(this.gain.gain,this.enabled&&!state.muted?p.levelPct/100*gain:0,state.muted?.01:.02);
  }
  diagnostics() {
    if(!this.analyser)return {context:'not-created',enabled:false,peak:0,rms:0};
    const samples=new Float32Array(this.analyser.fftSize);this.analyser.getFloatTimeDomainData(samples);
    return {context:this.context.state,enabled:this.enabled,peak:samples.reduce((v,x)=>Math.max(v,Math.abs(x)),0),rms:Math.sqrt(samples.reduce((v,x)=>v+x*x,0)/samples.length)};
  }
}
