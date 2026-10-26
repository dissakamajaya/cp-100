import {writeFileSync,readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
const directory='/Users/0xtx/Documents/GitHub/cp-100/evidence/audio-boost-2026-10-26';
export default async ({page,saveScreenshot}) => {
 const url=process.env.CP100_QA_URL||'http://127.0.0.1:8080/';
 const label=process.env.CP100_QA_LABEL||'local';
 await page.goto(url);
 const ready=await page.evaluate(async()=>{const end=Date.now()+15000;while(typeof window.chladniDiagnostics!=='function'&&Date.now()<end)await new Promise(r=>setTimeout(r,100));return typeof window.chladniDiagnostics==='function';});
 if(!ready)throw new Error('CP-100 app did not finish loading within 15 seconds');
 const snapshot=(await page.snapshot()).formattedTree;
 const dspHash=await page.evaluate(async()=>{const source=await(await fetch('./dsp.js')).arrayBuffer();return [...new Uint8Array(await crypto.subtle.digest('SHA-256',source))].map(x=>x.toString(16).padStart(2,'0')).join('');});
 const expectedHash=createHash('sha256').update(readFileSync('/Users/0xtx/Documents/GitHub/cp-100/simulator/dist/dsp.js')).digest('hex');
 const before=await page.evaluate(()=>window.chladniDiagnostics());
 await page.locator('#audio-toggle').click();
 const held=await page.evaluate(async()=>{
  document.body.dispatchEvent(new KeyboardEvent('keydown',{key:'1',code:'Digit1',bubbles:true}));
  await new Promise(r=>setTimeout(r,400));return window.chladniDiagnostics();
 });
 const released=await page.evaluate(async()=>{
  document.body.dispatchEvent(new KeyboardEvent('keyup',{key:'1',code:'Digit1',bubbles:true}));
  await new Promise(r=>setTimeout(r,400));return window.chladniDiagnostics();
 });
 await page.locator('#audio-toggle').click();
 const disabled=await page.evaluate(()=>window.chladniDiagnostics());
 const rendered=await page.evaluate(async()=>{
  const {AudioPreview}=await import('./dsp.js');
  const cases=[];
  for(const wave of ['sine','sawtooth','triangle'])for(const harmonics of ['odd','even','both'])for(const foldPct of [0,40,100])cases.push({frequencyHz:256,levelPct:100,wave,harmonics,foldPct});
  for(const frequencyHz of [20,2000])cases.push({frequencyHz,levelPct:100,wave:'sine',harmonics:'odd',foldPct:0});
  for(const levelPct of [0,35,50])cases.push({frequencyHz:256,levelPct,wave:'sine',harmonics:'odd',foldPct:0});
  const results=[];
  for(const parameters of cases){
   const context=new OfflineAudioContext(1,24000,48000),preview=new AudioPreview();preview.build(context);preview.enabled=true;
   preview.update({parameters,muted:false});
   const buffer=await context.startRendering(),samples=buffer.getChannelData(0).subarray(12000);
   let peak=0,squared=0,mean=0;for(const v of samples){peak=Math.max(peak,Math.abs(v));squared+=v*v;mean+=v;}
   results.push({parameters,rms:Math.sqrt(squared/samples.length),peak,mean:mean/samples.length});
  }
  return results;
 });
 const passed=dspHash===expectedHash&&before.state.muted&&!before.audio.enabled&&held.audio.context==='running'&&held.audio.rms>.1&&released.audio.rms<.00001&&released.state.muted&&!disabled.audio.enabled&&rendered.every(r=>r.peak<1&&(r.parameters.levelPct===0?r.rms===0:r.rms>.27*r.parameters.levelPct/100&&r.rms<.41*r.parameters.levelPct/100))&&rendered.filter(r=>r.parameters.wave==='sine'&&r.parameters.frequencyHz===256&&r.parameters.foldPct===0).every(r=>Math.abs(r.rms-.36*r.parameters.levelPct/100)<.002);
 const result={url,label,passed,dspHash,snapshot,before,held,released,disabled,rendered};
 writeFileSync(`${directory}/${label}.json`,JSON.stringify(result,null,2));
 await saveScreenshot(`${directory}/${label}.png`);
 if(!passed)throw new Error(`Audio QA failed; inspect ${directory}/${label}.json`);
 return {url,label,passed,heldRms:held.audio.rms,releasedRms:released.audio.rms,maxPeak:Math.max(...rendered.map(r=>r.peak)),renderedCases:rendered.length};
};
