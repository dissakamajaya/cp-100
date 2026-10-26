import test from 'node:test';
import assert from 'node:assert/strict';
import { padNumberForKey } from '../dist/controls.js';
import { initialState, reduce, parsePresetFile, exportPresetFile, DEFAULT_SETUP } from '../dist/model.js';

test('top-row and numpad 1–8 map to pads, including Num Lock off',()=>{
 const navigation=['End','ArrowDown','PageDown','ArrowLeft','Clear','ArrowRight','Home','ArrowUp'];
 for(let i=1;i<=8;i++){
  assert.equal(padNumberForKey({key:String(i),code:`Digit${i}`}),i);
  assert.equal(padNumberForKey({key:String(i),code:`Numpad${i}`}),i);
  assert.equal(padNumberForKey({key:navigation[i-1],code:`Numpad${i}`}),i);
 }
 for(const key of ['0','9','End','ArrowDown','F1',''])assert.equal(padNumberForKey({key,code:key}),null);
});
test('4000 Hz and fractional fold survive save/export/import and recall',()=>{
 let state=reduce(initialState(),{type:'MODE',mode:'SERVICE'});
 assert.equal(state.setup.maxHz,4000);
 state=reduce(state,{type:'PARAMETERS',patch:{frequencyHz:4000,foldPct:12.3}});
 assert.equal(state.parameters.frequencyHz,4000);assert.equal(state.parameters.foldPct,12.3);
 state=reduce(reduce(state,{type:'ARM'}),{type:'PAD_DOWN',index:7});
 const reboot=initialState(parsePresetFile(JSON.stringify(exportPresetFile(state.presets))));
 const recalled=reduce(reboot,{type:'PAD_DOWN',index:7});
 assert.equal(recalled.parameters.frequencyHz,4000);assert.equal(recalled.parameters.foldPct,12.3);
 assert.equal(recalled.muted,false);
 const service=reduce(recalled,{type:'MODE',mode:'SERVICE'});
 assert.equal(reduce(service,{type:'PARAMETERS',patch:{frequencyHz:4000.1}}),service);
});
test('existing 2000 Hz sweep bounds and calibrated custom limits stay intact',()=>{
 for(const setup of [{...DEFAULT_SETUP,maxHz:2000},{...DEFAULT_SETUP,minHz:150,maxHz:350,calibrated:true}]){
  let state=reduce(initialState(undefined,setup),{type:'MODE',mode:'SERVICE'});
  assert.deepEqual(state.setup,setup);
  state=reduce(state,{type:'PARAMETERS',patch:{frequencyHz:4000}});
  assert.equal(state.parameters.frequencyHz,setup.maxHz);
  state=reduce(state,{type:'SETUP',patch:{maxHz:4000}});
  assert.equal(state.setup.maxHz,4000);
  assert.equal(reduce(state,{type:'SETUP',patch:{maxHz:4001}}),state);
 }
});
