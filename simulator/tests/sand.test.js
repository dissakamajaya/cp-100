import test from 'node:test';import assert from 'node:assert/strict';
import {SandPreview} from '../dist/sand.js';import{DEFAULT_PARAMETERS}from'../dist/model.js';
function fixture(){
 const preview=Object.create(SandPreview.prototype);Object.assign(preview,{field:new Float32Array(81*81),gx:new Float32Array(81*81),gy:new Float32Array(81*81),x:new Float32Array(4400),y:new Float32Array(4400),vx:new Float32Array(4400),vy:new Float32Array(4400),seed:86173,version:0,spectrumKey:'',parameters:{...DEFAULT_PARAMETERS}});return preview;
}
test('sand field follows frequency, wave and harmonic selector and remains finite',()=>{
 const p=fixture();p.updateField();const initial=p.field.slice();
 p.parameters.frequencyHz=430;p.updateField();assert.ok(initial.some((v,i)=>Math.abs(v-p.field[i])>.01));
 p.parameters.foldPct=50;p.parameters.harmonics='even';p.updateField();const even=p.field.slice();p.parameters.harmonics='both';p.updateField();assert.ok(even.some((v,i)=>Math.abs(v-p.field[i])>.01));
 for(const a of[p.field,p.gx,p.gy])assert.ok(a.every(Number.isFinite));
});
test('sand animation keeps grains inside the glass window under prolonged excitation',()=>{
 const p=fixture();p.parameters={...DEFAULT_PARAMETERS,levelPct:100,frequencyHz:800,foldPct:90,harmonics:'both'};p.updateField();p.seedSand();
 for(let i=0;i<150;i++)p.step(i*30,30);
 for(let i=0;i<4400;i++){assert.ok(Number.isFinite(p.x[i])&&Number.isFinite(p.y[i]));assert.ok(Math.hypot(p.x[i],p.y[i])<.986);}
});
function distribution(p){
 const cells=new Set();let rim=0,near=0;
 for(let i=0;i<p.x.length;i++){
  const x=p.x[i],y=p.y[i];cells.add(`${Math.floor((x+1)*12)}:${Math.floor((y+1)*12)}`);
  if(Math.hypot(x,y)>.96)rim++;
  const distance=Math.abs(p.sample(p.field,x,y))/Math.max(.15,Math.hypot(p.sample(p.gx,x,y),p.sample(p.gy,x,y)));
  if(distance<.065)near++;
 }
 return {cells:cells.size,rim,near:near/p.x.length};
}
test('settled sand keeps granular nodal bands across presets without piling onto the rim',()=>{
 for(const frequencyHz of [128.5,196,256,384,800,2000]){
  const p=fixture();p.parameters.frequencyHz=frequencyHz;p.updateField();p.seedSand();
  for(let i=0;i<600;i++)p.step(i*30,30);
  const d=distribution(p);assert.ok(d.cells>90,`${frequencyHz}: ${JSON.stringify(d)}`);assert.ok(d.rim<20);assert.ok(d.near>.8,`${frequencyHz}: ${JSON.stringify(d)}`);
 }
});
test('frequency change moves existing grains toward a new pattern rather than teleporting them',()=>{
 const p=fixture();p.updateField();p.seedSand();const before=p.x.slice();p.parameters.frequencyHz=384;p.updateField();
 assert.deepEqual(p.x,before);assert.ok(p.tx.some((x,i)=>Math.abs(x-before[i])>.2));
 p.step(30,30);assert.ok(p.x.some((x,i)=>x!==before[i]));
 for(let i=1;i<180;i++)p.step(i*30,30);
 assert.ok(distribution(p).near>.8);
});
test('scene geometry matches the final 60 cm plate and 50 cm glass',async()=>{
 const {SPEC}=await import('../dist/model.js');const {SCENE}=await import('../dist/sand.js');
 assert.equal(SPEC.plateCm,60);assert.equal(SPEC.glassDiameterCm,50);
 assert.ok(Math.abs(SCENE.windowHalf-5/12)<1e-12);
 assert.ok(Math.abs(SCENE.radiusFraction*2/.94-50/60)<1e-12);
});
