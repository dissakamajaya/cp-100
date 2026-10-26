import test from 'node:test';
import assert from 'node:assert/strict';
import { initialState, reduce, exportPresetFile, parsePresetFile } from '../dist/model.js';
import { period, foldSample } from '../dist/dsp.js';

test('RUN cannot change parameters or arm SAVE', () => {
  const run = initialState();
  assert.equal(reduce(run, { type: 'PARAMETERS', patch: { foldPct: 90 } }), run);
  assert.equal(reduce(run, { type: 'ARM' }), run);
});
test('SAVE captures five independent parameters, persists, and recalls after a muted reboot', () => {
  let state = reduce(initialState(), { type: 'MODE', mode: 'SERVICE' });
  const expected = { frequencyHz: 432.1, levelPct: 47, wave: 'triangle', foldPct: 63, harmonics: 'both' };
  state = reduce(state, { type: 'PARAMETERS', patch: expected });
  state = reduce(state, { type: 'ARM' }); state = reduce(state, { type: 'PAD', index: 7 });
  const file = JSON.stringify(exportPresetFile(state.presets));
  let reboot = initialState(parsePresetFile(file));
  assert.equal(reboot.mode, 'RUN'); assert.equal(reboot.muted, true); assert.equal(reboot.activePad, null);
  reboot = reduce(reboot, { type: 'PAD', index: 7 });
  assert.deepEqual(reboot.parameters, expected); assert.equal(reboot.muted, false);
});
test('leaving SERVICE cancels an armed save and mutes; a subsequent pad recalls rather than overwrites', () => {
  let state = reduce(initialState(), { type: 'MODE', mode: 'SERVICE' });
  state = reduce(state, { type: 'PARAMETERS', patch: { frequencyHz: 1000 } });
  state = reduce(state, { type: 'ARM' }); state = reduce(state, { type: 'MODE', mode: 'RUN' });
  assert.equal(state.armed, false); assert.equal(state.muted, true);
  assert.equal(reduce(state, { type: 'PAD', index: 0 }).parameters.frequencyHz, 128.5);
});
test('empty pad does not start or alter output', () => {
  const state = initialState(); assert.equal(reduce(state, { type: 'PAD', index: 6 }), state);
});
test('invalid import is atomic and rejects unsupported waves, bad slot counts, and nonfinite levels', () => {
  const state = reduce(initialState(), { type: 'MODE', mode: 'SERVICE' });
  for (const mutate of [x => x.presets.pop(), x => { x.presets[0].wave = 'square'; }, x => { x.presets[0].levelPct = Infinity; }, x => { x.presets[0].foldPct = -1; }, x => { x.presets[0].harmonics = 'evenOnly'; }, x => { x.version = 2; }]) {
    const file = exportPresetFile(state.presets); mutate(file);
    assert.throws(() => parsePresetFile(file));
    assert.equal(reduce(state, { type: 'IMPORT', value: file }), state);
  }
});
test('fold zero is an exact bypass and folding remains odd and bounded', () => {
  
  for (let i = -100; i <= 100; i++) {
    const x = i / 100; assert.equal(foldSample(x, 0), x);
    for (const amount of [1, 50, 100]) { assert.ok(Math.abs(foldSample(x, amount)) <= 1); assert.ok(Math.abs(foldSample(x, amount) + foldSample(-x, amount)) < 1e-12); }
  }
});
test('normalization holds the signal RMS across waves and folding while preserving peak headroom and zero DC', () => {
  for (const wave of ['sine', 'sawtooth', 'triangle']) for (const foldPct of [0, 1, 40, 100]) for (const harmonics of ['odd', 'even', 'both']) {
    const values = period({ frequencyHz: 128.5, levelPct: 80, wave, foldPct, harmonics }, 4096);
    const rms = Math.sqrt(values.reduce((s, x) => s + x * x, 0) / values.length);
    const mean = values.reduce((s, x) => s + x, 0) / values.length;
    assert.ok(Math.abs(rms - .288) < 1e-6, `${wave}/${foldPct}: ${rms}`);
    assert.ok(Math.abs(mean) < .0003); assert.ok(Math.max(...values.map(Math.abs)) < .8);
  }
});
test('folding a sine adds a measurable third harmonic', () => {
  function amplitude(samples, harmonic) {
    let a = 0, b = 0;
    for (let i = 0; i < samples.length; i++) { const t = 2 * Math.PI * harmonic * i / samples.length; a += samples[i] * Math.sin(t); b += samples[i] * Math.cos(t); }
    return 2 * Math.hypot(a, b) / samples.length;
  }
  const clean = period({ frequencyHz: 100, levelPct: 50, wave: 'sine', foldPct: 0, harmonics: 'odd' }, 4096);
  const shaped = period({ frequencyHz: 100, levelPct: 50, wave: 'sine', foldPct: 70, harmonics: 'odd' }, 4096);
  assert.ok(amplitude(clean, 3) < 1e-6); assert.ok(amplitude(shaped, 3) / amplitude(shaped, 1) > .1);
});

test('asymmetric folding adds even harmonics and removes DC; symmetric sine remains odd', () => {
  function amplitude(samples, n) { let re=0, im=0; samples.forEach((x,i)=>{re+=x*Math.cos(2*Math.PI*n*i/samples.length);im+=x*Math.sin(2*Math.PI*n*i/samples.length);}); return 2*Math.hypot(re,im)/samples.length; }
  for (const foldPct of [25, 50, 70]) {
    const p = { frequencyHz: 100, levelPct: 100, wave: 'sine', foldPct };
    const odd = period({...p, harmonics:'odd'},4096), even = period({...p, harmonics:'both'},4096);
    assert.ok(amplitude(odd,2)<1e-6); assert.ok(amplitude(even,2)>.005);
    assert.ok(Math.abs(even.reduce((a,b)=>a+b,0)/even.length)<1e-6);
  }
});
test('folding really reflects after the threshold instead of saturating', () => {
  assert.ok(foldSample(.5,25) > foldSample(.75,25));
  assert.ok(foldSample(.9,25) < 0);
});

test('EVEN adds even harmonics while retaining the sine fundamental and no added odd harmonics', () => {
  function amp(s,n) { let re=0,im=0; s.forEach((x,i)=>{re+=x*Math.cos(2*Math.PI*n*i/s.length);im+=x*Math.sin(2*Math.PI*n*i/s.length);}); return 2*Math.hypot(re,im)/s.length; }
  const p={frequencyHz:128.5,levelPct:50,wave:'sine',foldPct:50};
  const even=period({...p,harmonics:'even'},4096), both=period({...p,harmonics:'both'},4096);
  assert.ok(amp(even,1)>.1);assert.ok(amp(even,2)>.005);assert.ok(amp(even,3)<1e-6);assert.ok(amp(both,2)>.005&&amp(both,3)>.005);
});
test('version 3 presets migrate explicitly to three-position harmonics without losing slot data', () => {
  const old={schema:'chladni-08',version:3,folding:'triangle-fold-v1',presets:Array(8).fill(null)};
  old.presets[0]={frequencyHz:432.1,levelPct:47,wave:'triangle',foldPct:63,foldMode:'asymmetric'};
  const migrated=parsePresetFile(old);
  assert.deepEqual(migrated[0],{frequencyHz:432.1,levelPct:47,wave:'triangle',foldPct:63,harmonics:'both'});
  assert.equal(migrated[7],null);assert.equal(exportPresetFile(migrated).version,4);
});

test('momentary pad release mutes, clears active pad, and cannot resume via RUN mute button',()=>{
 let state=reduce(initialState(),{type:'PAD_DOWN',index:0});assert.equal(state.muted,false);assert.equal(state.activePad,0);
 state=reduce(state,{type:'PAD_UP',index:0});assert.equal(state.muted,true);assert.equal(state.activePad,null);
 assert.equal(reduce(state,{type:'MUTE',value:false}),state);
});
test('first held pad owns output; other press and release cannot steal or silence it',()=>{
 let state=reduce(initialState(),{type:'PAD_DOWN',index:0});
 assert.equal(reduce(state,{type:'PAD_DOWN',index:1}),state);assert.equal(reduce(state,{type:'PAD_UP',index:1}),state);
 state=reduce(state,{type:'RELEASE_ALL'});assert.equal(state.muted,true);assert.equal(state.activePad,null);
});
test('commissioned sweep limits clamp editing, reject excluded presets and survive reboot',()=>{
 let state=reduce(initialState(),{type:'MODE',mode:'SERVICE'});
 state=reduce(state,{type:'SETUP',patch:{minHz:150,maxHz:350,calibrated:true}});
 assert.equal(state.parameters.frequencyHz,150);assert.equal(state.muted,true);
 state=reduce(state,{type:'PARAMETERS',patch:{frequencyHz:20}});assert.equal(state.parameters.frequencyHz,150);
 state=reduce(state,{type:'PARAMETERS',patch:{frequencyHz:2000}});assert.equal(state.parameters.frequencyHz,350);
 assert.equal(reduce(state,{type:'PAD_DOWN',index:0}),state);
 state=reduce(state,{type:'PAD_DOWN',index:1});assert.equal(state.parameters.frequencyHz,196);assert.equal(state.muted,false);
 const reboot=initialState(state.presets,JSON.parse(JSON.stringify(state.setup)));assert.deepEqual(reboot.setup,state.setup);assert.equal(reboot.muted,true);
 const invalid=reduce(reboot,{type:'MODE',mode:'SERVICE'});assert.equal(reduce(invalid,{type:'SETUP',patch:{minHz:400,maxHz:100}}),invalid);
});
test('RGB pads are white at standby, pink/tosca while held, and optional pulse stays in visual range',async()=>{
 const {ledState}=await import('../dist/model.js');let state=initialState();assert.equal(ledState(state,0).color,'white');
 state=reduce(state,{type:'PAD_DOWN',index:0});assert.equal(ledState(state,0).color,'pink');assert.equal(ledState(state,1).color,'white');
 state=reduce(state,{type:'PAD_UP',index:0});state=reduce(state,{type:'PAD_DOWN',index:1});assert.equal(ledState(state,1).color,'tosca');
 state=reduce(state,{type:'MODE',mode:'SERVICE'});state=reduce(state,{type:'SETUP',patch:{ledPulse:true}});state=reduce(state,{type:'PAD_DOWN',index:0});
 assert.ok(ledState(state,0).pulseHz>=.5&&ledState(state,0).pulseHz<=2);
 state=reduce(state,{type:'RELEASE_ALL'});assert.equal(ledState(state,0).pulseHz,0);assert.equal(ledState(state,0).color,'white');
});
