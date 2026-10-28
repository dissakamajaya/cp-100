import test from 'node:test';
import assert from 'node:assert/strict';
import { GatherPlayer, gatherAt, gatherDuration, cleanGather, exampleGather, exportGatherFile, parseGatherFile, GATHER_FADE_S, GATHER_FRAME_MS } from '../dist/gather.js';
import { initialState, reduce, DEFAULT_SETUP } from '../dist/model.js';
class Clock {
 constructor(){this.now=0;this.next=0;this.tasks=new Map();}
 time=()=>this.now;
 schedule=(callback,delay)=>{const id=++this.next;this.tasks.set(id,{callback,at:this.now+delay});return id;};
 cancel=id=>this.tasks.delete(id);
 tick(ms){const end=this.now+ms;for(;;){const next=[...this.tasks].filter(([,t])=>t.at<=end).sort((a,b)=>a[1].at-b[1].at)[0];if(!next)break;this.now=next[1].at;this.tasks.delete(next[0]);next[1].callback();}this.now=end;}
}
const steps=[{frequencyHz:50,levelPct:40,glideS:2,holdS:3},{frequencyHz:200,levelPct:20,glideS:4,holdS:1}];
test('first glide fades in, later glides move exponentially in frequency and linearly in level, then fade out',()=>{
 assert.deepEqual(gatherAt(steps,0),{index:0,phase:'glide',frequencyHz:50,levelPct:0});
 assert.equal(gatherAt(steps,1).levelPct,20);
 assert.deepEqual(gatherAt(steps,2.5),{index:0,phase:'hold',frequencyHz:50,levelPct:40});
 const middle=gatherAt(steps,7);assert.equal(middle.phase,'glide');assert.equal(middle.index,1);
 assert.ok(Math.abs(middle.frequencyHz-100)<1e-9);assert.ok(Math.abs(middle.levelPct-30)<1e-9);
 assert.deepEqual(gatherAt(steps,9.5),{index:1,phase:'hold',frequencyHz:200,levelPct:20});
 assert.equal(gatherAt(steps,10+GATHER_FADE_S/2).levelPct,10);
 assert.equal(gatherDuration(steps),10+GATHER_FADE_S);assert.equal(gatherAt(steps,gatherDuration(steps)),null);
});
test('zero-length glides jump straight to the hold value',()=>{
 const jump=[{frequencyHz:60,levelPct:30,glideS:0,holdS:1},{frequencyHz:90,levelPct:25,glideS:0,holdS:1}];
 assert.equal(gatherAt(jump,0).levelPct,30);assert.equal(gatherAt(jump,1).frequencyHz,90);
});
test('validation rejects bad routines atomically and the file round-trips',()=>{
 for(const bad of [[],Array(17).fill(steps[0]),[{...steps[0],frequencyHz:10}],[{...steps[0],levelPct:NaN}],[{...steps[0],glideS:-1}],[{...steps[0],holdS:61}],Array(6).fill({...steps[0],glideS:60,holdS:0})])
  assert.throws(()=>cleanGather(bad));
 assert.deepEqual(parseGatherFile(JSON.stringify(exportGatherFile(exampleGather()))),exampleGather());
 assert.throws(()=>parseGatherFile({schema:'chladni-08',version:1,steps}));
});
test('player plays once at frame rate, then reports done and stops scheduling',()=>{
 const clock=new Clock(),frames=[];let done=0;
 const player=new GatherPlayer({onFrame:f=>frames.push(f),onDone:()=>done++,now:clock.time,schedule:clock.schedule,cancel:clock.cancel});
 player.start(steps);clock.tick(gatherDuration(steps)*1000+500);
 assert.equal(done,1);assert.equal(player.running,false);assert.equal(clock.tasks.size,0);
 assert.ok(frames.length>=gatherDuration(steps)*1000/GATHER_FRAME_MS-1);
 assert.deepEqual([...new Set(frames.map(f=>f.index))],[0,1]);
});
test('stop cancels pending frames, including stale callbacks, without calling done',()=>{
 const clock=new Clock(),frames=[];let done=0;
 const player=new GatherPlayer({onFrame:f=>frames.push(f),onDone:()=>done++,now:clock.time,schedule:clock.schedule,cancel:clock.cancel});
 player.start(steps);const stale=[...clock.tasks.values()][0].callback;player.stop();stale();clock.tick(20000);
 assert.equal(frames.length,1);assert.equal(done,0);assert.equal(clock.tasks.size,0);
 assert.throws(()=>player.start([]));assert.equal(player.running,false);
});
test('GATHER_FRAME drives a plain sine in SERVICE only, clamps to sweep limits and leaves presets alone',()=>{
 const run=initialState();assert.equal(reduce(run,{type:'GATHER_FRAME',frequencyHz:100,levelPct:30}),run);
 let state=reduce(initialState(undefined,{...DEFAULT_SETUP,maxHz:250}),{type:'MODE',mode:'SERVICE'});
 state=reduce(state,{type:'PARAMETERS',patch:{wave:'sawtooth',foldPct:40,harmonics:'both'}});
 state=reduce(state,{type:'ARM'});const presets=state.presets;
 state=reduce(state,{type:'GATHER_FRAME',frequencyHz:300,levelPct:22.4});
 assert.deepEqual(state.parameters,{frequencyHz:250,levelPct:22,wave:'sine',foldPct:0,harmonics:'odd'});
 assert.equal(state.muted,false);assert.equal(state.armed,false);assert.equal(state.activePad,null);assert.equal(state.presets,presets);
 assert.equal(reduce(state,{type:'GATHER_FRAME',frequencyHz:NaN,levelPct:20}),state);
});
