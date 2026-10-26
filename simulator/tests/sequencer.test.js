import test from 'node:test';
import assert from 'node:assert/strict';
import { PadSequencer } from '../dist/sequencer.js';
import { initialState, reduce, DEFAULT_SETUP } from '../dist/model.js';
class Clock {
 constructor(){this.now=0;this.next=0;this.tasks=new Map();}
 schedule=(callback,delay)=>{const id=++this.next;this.tasks.set(id,{callback,at:this.now+delay});return id;};
 cancel=id=>this.tasks.delete(id);
 tick(ms){const end=this.now+ms;for(;;){const next=[...this.tasks].filter(([,t])=>t.at<=end).sort((a,b)=>a[1].at-b[1].at)[0];if(!next)break;this.now=next[1].at;this.tasks.delete(next[0]);next[1].callback();}this.now=end;}
}
function fixture(onStep,random){const clock=new Clock();const sequence=new PadSequencer({onStep,random,schedule:clock.schedule,cancel:clock.cancel});return {clock,sequence};}
test('ordered sequence starts at pad 1, holds each for 3000 ms and loops all eight',()=>{
 const steps=[];const {clock,sequence}=fixture(i=>steps.push({i,time:clock.now}));
 sequence.start();clock.tick(2999);assert.deepEqual(steps,[{i:0,time:0}]);
 clock.tick(1);assert.equal(steps[1].i,1);clock.tick(21000);
 assert.deepEqual(steps.map(x=>x.i),[0,1,2,3,4,5,6,7,0]);
 assert.deepEqual(steps.map(x=>x.time),[0,3000,6000,9000,12000,15000,18000,21000,24000]);
 sequence.stop();assert.equal(clock.tasks.size,0);
});
test('random mode visits every pad once per cycle and avoids immediate repeats across cycles',()=>{
 let seed=7;const random=()=>((seed=(seed*1664525+1013904223)>>>0)/2**32);
 const steps=[];const {clock,sequence}=fixture(i=>steps.push(i),random);
 sequence.start('random');clock.tick(3000*31);
 for(let i=0;i<32;i+=8)assert.deepEqual([...steps.slice(i,i+8)].sort(),[0,1,2,3,4,5,6,7]);
 assert.notDeepEqual(steps.slice(0,8),[0,1,2,3,4,5,6,7]);
 for(let i=1;i<steps.length;i++)assert.notEqual(steps[i],steps[i-1]);
 sequence.stop();
});
test('stop and restart cancel pending ticks, including callbacks already queued by the event loop',()=>{
 const steps=[];const {clock,sequence}=fixture(i=>steps.push(i));sequence.start();
 const stale=[...clock.tasks.values()][0].callback;
 sequence.stop();stale();clock.tick(6000);assert.deepEqual(steps,[0]);assert.equal(sequence.diagnostics().index,null);
 sequence.start();stale();assert.deepEqual(steps,[0,0]);assert.equal(clock.tasks.size,1);
 clock.tick(3000);assert.deepEqual(steps,[0,0,1]);sequence.stop();
 assert.throws(()=>sequence.start('invalid'));assert.equal(clock.tasks.size,0);
});
test('empty and excluded slots are silent for their full step and never keep the previous pad playing',()=>{
 let state=initialState(undefined,{...DEFAULT_SETUP,maxHz:250});const states=[];
 const {clock,sequence}=fixture(index=>{
  state=reduce(state,{type:'SEQUENCE_STEP',index});states.push({index,muted:state.muted,activePad:state.activePad});
 });
 sequence.start();clock.tick(21000);
 assert.deepEqual(states.slice(0,2),[{index:0,muted:false,activePad:0},{index:1,muted:false,activePad:1}]);
 for(const step of states.slice(2)){assert.equal(step.muted,true);assert.equal(step.activePad,null);}
 sequence.stop();
});
test('a step callback can stop playback without scheduling another tick',()=>{
 const {clock,sequence}=fixture(()=>sequence.stop());sequence.start();assert.equal(clock.tasks.size,0);assert.equal(sequence.running,false);
});

test('sequence steps disarm SAVE without changing presets and replace the current pad atomically',()=>{
 let state=reduce(initialState(),{type:'MODE',mode:'SERVICE'});
 state=reduce(state,{type:'ARM'});const presets=state.presets;
 state=reduce(state,{type:'SEQUENCE_STEP',index:0});
 assert.equal(state.armed,false);assert.equal(state.activePad,0);assert.equal(state.muted,false);assert.equal(state.presets,presets);
 state=reduce(state,{type:'SEQUENCE_STEP',index:1});assert.equal(state.activePad,1);assert.equal(state.muted,false);
 state=reduce(state,{type:'SEQUENCE_STEP',index:99});assert.equal(state.muted,true);assert.equal(state.activePad,null);
});
