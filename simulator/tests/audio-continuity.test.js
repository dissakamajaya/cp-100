import test from 'node:test';
import assert from 'node:assert/strict';
import { AudioPreview } from '../dist/dsp.js';
import { DEFAULT_PARAMETERS } from '../dist/model.js';
class Parameter {
  constructor(value=0){this.value=value;this.events=[];}
  cancelAndHoldAtTime(time){this.events.push({type:'hold',time});}
  setTargetAtTime(value,time,seconds){this.value=value;this.events.push({type:'target',value,time,seconds});}
}
class Node {
  constructor(){this.gain=new Parameter(1);this.frequency=new Parameter(440);this.offset=new Parameter(1);this.Q=new Parameter(1);this.curveWrites=0;this.starts=0;}
  connect(other){return other;}
  start(){this.starts++;}
  set curve(value){this._curve=value;this.curveWrites++;}
  get curve(){return this._curve;}
}
class Context {
  constructor(){this.currentTime=0;this.destination=new Node();this.nodes=[];this.state='running';}
  node(){const n=new Node();this.nodes.push(n);return n;}
  createGain(){return this.node();}createOscillator(){return this.node();}createWaveShaper(){return this.node();}createConstantSource(){return this.node();}createBiquadFilter(){return this.node();}createAnalyser(){return this.node();}
}
test('rapid continuous sweep automates frequency without zeroing gain, replacing curves, restarting oscillators, or rebuilding nodes',()=>{
  const preview=new AudioPreview(),context=new Context();preview.build(context);preview.enabled=true;
  const state={parameters:{...DEFAULT_PARAMETERS},muted:false};preview.update(state);
  const nodeCount=context.nodes.length;preview.gain.gain.events=[];
  for(let i=1;i<=120;i++){
    context.currentTime=i*.012;
    preview.update({...state,parameters:{...state.parameters,frequencyHz:128.5+i}});
  }
  assert.equal(context.nodes.length,nodeCount);
  assert.ok(preview.oscillators.every(({oscillator})=>oscillator.starts===1));
  assert.ok([preview.odd,preview.dry,preview.plus,preview.minus].every(({folder})=>folder.curveWrites===1));
  assert.equal(preview.gain.gain.events.filter(e=>e.type==='target'&&e.value===0).length,0);
  const events=preview.oscillators[0].oscillator.frequency.events.filter(e=>e.type==='target');
  assert.equal(events.length,121);assert.ok(events.every(e=>e.seconds>0));assert.equal(events.at(-1).value,248.5);
});
test('folder and three-position selector adjust live AudioParams; MUTE and audio-off alone target zero output',()=>{
  const preview=new AudioPreview();preview.build(new Context());preview.enabled=true;
  for(const harmonics of ['odd','even','both'])for(const foldPct of [0,25,70,100]){
    preview.update({parameters:{...DEFAULT_PARAMETERS,harmonics,foldPct},muted:false});
    assert.ok(preview.gain.gain.value>0);assert.ok(preview.plus.folder.curveWrites===1);
    assert.equal(preview.plus.mix.gain.value,harmonics==='odd'?0:.5);
    assert.equal(preview.dry.mix.gain.value,harmonics==='even'?1:0);
  }
  preview.update({parameters:{...DEFAULT_PARAMETERS},muted:true});assert.equal(preview.gain.gain.value,0);
  preview.update({parameters:{...DEFAULT_PARAMETERS},muted:false});assert.ok(preview.gain.gain.value>0);
  preview.disable();assert.equal(preview.gain.gain.value,0);
});
