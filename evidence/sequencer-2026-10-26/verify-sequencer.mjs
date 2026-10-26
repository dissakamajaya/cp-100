import {writeFileSync,readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
const directory='/Users/0xtx/Documents/GitHub/cp-100/evidence/sequencer-2026-10-26';
export default async({page,saveScreenshot})=>{
 const url=process.env.CP100_QA_URL||'http://127.0.0.1:8080/',label=process.env.CP100_QA_LABEL||'local',checks=[];
 const check=(name,passed)=>{checks.push({name,passed:!!passed});if(!passed)throw new Error(name);};
 const wait=ms=>page.evaluate(ms=>new Promise(r=>setTimeout(r,ms)),ms);
 const diag=()=>page.evaluate(()=>window.chladniDiagnostics());
 const play=async()=>{await page.locator('#sequence-play').click();await page.evaluate(async()=>{const end=performance.now()+4000;while(window.chladniDiagnostics().sequence.pending&&performance.now()<end)await new Promise(r=>setTimeout(r,20));});await wait(200);};
 const load=async()=>{await page.goto(url);await page.waitForLoadState('load');const ready=await page.evaluate(async()=>{const end=Date.now()+15000;while(typeof window.chladniDiagnostics!=='function'&&Date.now()<end)await new Promise(r=>setTimeout(r,50));return typeof window.chladniDiagnostics==='function';});check('App loaded',ready);await page.snapshot();};
 const capture=duration=>page.evaluate(async duration=>{
  const events=[],start=performance.now();let last=null;
  await new Promise(resolve=>{const timer=setInterval(()=>{const d=window.chladniDiagnostics();if(d.sequence.index!==last){last=d.sequence.index;events.push({index:last,at:performance.now()-start,muted:d.state.muted,activePad:d.state.activePad});}if(performance.now()-start>=duration){clearInterval(timer);resolve();}},20);});
  return events;
 },duration);
 try{
  await load();await page.setViewportSize(1280,900);
  check('No autoplay on load',!(await diag()).sequence.running&&(await diag()).state.muted);
  await play();let d=await diag();
  check('Play enables actual audio and immediately starts pad 1',d.sequence.running&&d.sequence.index===0&&d.state.activePad===0&&d.audio.enabled&&d.audio.rms>.05);
  const ordered=await capture(24100);
  check('Ordered 1–8 loops to 1',ordered.length===9&&ordered.every((e,i)=>e.index===i%8));
  check('Each step takes three seconds',ordered.slice(2).every((e,i)=>Math.abs(e.at-ordered[i+1].at-3000)<250));
  check('Empty slots stay muted',ordered.filter(e=>e.index>=4).every(e=>e.muted&&e.activePad===null));
  await saveScreenshot(`${directory}/${label}-desktop.png`);
  await play();d=await diag();check('Stop mutes output',!d.sequence.running&&d.state.muted&&d.audio.rms<.00001);
  await wait(3100);check('Stopped timer cannot restart',!(await diag()).sequence.running&&(await diag()).state.muted);
  await page.locator('#sequence-order').selectOption('random');await play();
  const random=await capture(21200);
  check('Random visits each pad exactly once per cycle',random.length===8&&new Set(random.map(e=>e.index)).size===8);
  check('Random also uses three-second steps',random.slice(2).every((e,i)=>Math.abs(e.at-random[i+1].at-3000)<250));
  await page.setViewportSize(390,844);await page.evaluate(()=>document.querySelector('.sequencer').scrollIntoView({block:'center'}));
  check('Mobile has no horizontal overflow',await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  await saveScreenshot(`${directory}/${label}-mobile.png`);
  await page.evaluate(()=>document.body.dispatchEvent(new KeyboardEvent('keydown',{key:'1',code:'Numpad1',bubbles:true})));
  d=await diag();check('Manual pad takes over and stops the timer',!d.sequence.running&&d.state.activePad===0);
  await page.evaluate(()=>document.body.dispatchEvent(new KeyboardEvent('keyup',{key:'1',code:'Numpad1',bubbles:true})));
  check('Manual key release mutes',(await diag()).state.muted);
  await play();await page.locator('#sequence-order').selectOption('ordered');check('Order change stops playback',!(await diag()).sequence.running&&(await diag()).state.muted);
  await play();await page.locator('#maintenance-toggle').click();check('Opening service stops playback',!(await diag()).sequence.running&&(await diag()).state.muted);
  await page.locator('#save-action').click();const saved=JSON.stringify((await diag()).state.presets);
  await play();d=await diag();check('Play cancels SAVE without overwriting a preset',d.sequence.running&&!d.state.armed&&JSON.stringify(d.state.presets)===saved);
  await page.locator('#frequency-input').fill('500');await page.locator('#frequency-form button').click();
  d=await diag();check('Manual edit stops sequence',!d.sequence.running&&d.state.muted&&d.state.parameters.frequencyHz===500);
  await play();await page.locator('#mute-action').click();check('TEST/MUTE stops sequence',!(await diag()).sequence.running&&(await diag()).state.muted);
  await play();await page.locator('#audio-toggle').click();d=await diag();check('Audio OFF stops and mutes',!d.sequence.running&&!d.audio.enabled&&d.state.muted);
  await play();await page.evaluate(()=>window.dispatchEvent(new Event('blur')));check('Leaving page stops sequence',!(await diag()).sequence.running&&(await diag()).state.muted);
  await wait(3100);check('Blur cannot leave a later timer playing',!(await diag()).sequence.running&&(await diag()).state.muted);
  await load();check('Reload never resumes sequence',!(await diag()).sequence.running&&(await diag()).state.muted);
  // Hold audio activation pending, press Stop, then allow resume to complete.
  await page.evaluate(()=>{window.originalResume=AudioContext.prototype.resume;AudioContext.prototype.resume=function(){const context=this;return new Promise((resolve,reject)=>{window.finishResume=()=>window.originalResume.call(context).then(resolve,reject);});};});
  await page.locator('#sequence-play').click();check('Pending activation exposes Stop',(await diag()).sequence.pending);
  await page.locator('#sequence-play').click();await page.evaluate(()=>{AudioContext.prototype.resume=window.originalResume;window.finishResume();});await page.evaluate(async()=>{const end=performance.now()+4000;while(!window.chladniDiagnostics().audio.enabled&&performance.now()<end)await new Promise(r=>setTimeout(r,20));});
  d=await diag();check('Late audio activation cannot restart a stopped sequence',!d.sequence.running&&!d.sequence.pending&&d.state.muted&&d.audio.enabled);
  const hashes={};for(const name of ['app.js','index.html','styles.css','sequencer.js','model.js','dsp.js']){
   const actual=await page.evaluate(async name=>await(await fetch(`./${name}`)).text(),name);hashes[name]=createHash('sha256').update(actual).digest('hex');
   check(`Source hash matches: ${name}`,hashes[name]===createHash('sha256').update(readFileSync(`/Users/0xtx/Documents/GitHub/cp-100/simulator/dist/${name}`)).digest('hex'));
  }
  writeFileSync(`${directory}/${label}.json`,JSON.stringify({url,label,passed:true,checks,ordered,random,hashes},null,2));return {url,label,passed:true,checks:checks.length,orderedSteps:ordered.length,randomSteps:random.length};
 }catch(error){writeFileSync(`${directory}/${label}-failure.json`,JSON.stringify({url,label,checks,diagnostics:await diag().catch(()=>null),error:String(error)},null,2));throw error;}
};
