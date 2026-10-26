import {writeFileSync,readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
const directory='/Users/0xtx/Documents/GitHub/cp-100/evidence/controls-2026-10-26';
export default async({page,saveScreenshot})=>{
 const url=process.env.CP100_QA_URL||'http://127.0.0.1:8080/',label=process.env.CP100_QA_LABEL||'local',checks=[];
 const check=(name,value)=>{checks.push({name,passed:!!value});if(!value)throw new Error(name);};
 const state=()=>page.evaluate(()=>window.chladniDiagnostics().state);
 const ready=()=>page.evaluate(async()=>{const end=Date.now()+15000;while(typeof window.chladniDiagnostics!=='function'&&Date.now()<end)await new Promise(r=>setTimeout(r,100));return typeof window.chladniDiagnostics==='function';});
 try{
  await page.goto(url);await page.waitForLoadState('load');check('App loaded',await ready());await page.snapshot();
  check('Muted RUN boot', (await state()).muted&&(await state()).mode==='RUN');
  if(await page.evaluate(()=>document.querySelector('#service-panel').hidden))await page.locator('#maintenance-toggle').click();await page.snapshot();
  check('Default range ends at 4000 Hz',(await state()).setup.maxHz===4000);
  await page.locator('#frequency-input').fill('');await page.type('1234.5');
  check('Typing does not trigger pads',(await state()).activePad===null&&(await state()).muted);
  await page.keyPress('Enter');check('Enter applies typed decimal frequency',(await state()).parameters.frequencyHz===1234.5);
  await page.locator('#frequency-input').fill('4000');await page.locator('#frequency-form button').click();
  check('4000 Hz applies via button',(await state()).parameters.frequencyHz===4000);
  for(const value of ['4000.1','19.9','']){
   await page.locator('#frequency-input').fill(value);await page.locator('#frequency-form button').click();
   check(`Invalid frequency rejected: ${value||'empty'}`,(await state()).parameters.frequencyHz===4000);
  }
  await page.locator('#frequency-input').fill('4000');
  await page.locator('[data-knob="foldPct"]').click();await page.keyPress('ArrowUp');
  check('FOLD arrow step is 0.1%',(await state()).parameters.foldPct===.1);
  const point=await page.locator('[data-knob="foldPct"]').centroid();
  await page.scroll(point.x,point.y,0,-50);
  check('FOLD wheel step is 0.1%',(await state()).parameters.foldPct===.2);
  await page.dragAndDrop(point.x,point.y,point.x,point.y-20);
  check('FOLD drag is ten times finer',(await state()).parameters.foldPct===1.2);
  const shortcuts=await page.evaluate(()=>{
   const results=[],body=document.body;
   const send=(type,key,code,extra={})=>body.dispatchEvent(new KeyboardEvent(type,{key,code,bubbles:true,cancelable:true,...extra}));
   const read=()=>window.chladniDiagnostics().state;
   // Save the current typed frequency and fractional FOLD to each pad using the UI.
   for(let i=1;i<=8;i++){document.querySelector('#save-action').click();send('keydown',String(i),`Numpad${i}`);send('keyup',String(i),`Numpad${i}`);}
   for(let i=1;i<=8;i++)for(const [key,code] of [[String(i),`Digit${i}`],[String(i),`Numpad${i}`],[['End','ArrowDown','PageDown','ArrowLeft','Clear','ArrowRight','Home','ArrowUp'][i-1],`Numpad${i}`]]){
    send('keydown',key,code);const active=read().activePad;send('keyup',key,code);results.push({i,key,code,passed:active===i-1&&read().muted&&read().activePad===null});
   }
   send('keydown','1','Digit1');send('keydown','1','Numpad1');send('keyup','1','Numpad1');const owner=read().activePad===0;send('keyup','1','Digit1');
   const input=document.querySelector('#frequency-input');input.focus();input.dispatchEvent(new KeyboardEvent('keydown',{key:'2',code:'Numpad2',bubbles:true}));const typing=read().activePad===null;input.dispatchEvent(new KeyboardEvent('keyup',{key:'2',code:'Numpad2',bubbles:true}));
   send('keydown','2','Numpad2',{ctrlKey:true});const modified=read().activePad===null;
   send('keydown','2','Numpad2',{repeat:true});const repeat=read().activePad===null;
   return {results,owner,typing,modified,repeat,presets:read().presets};
  });
  check('All 24 top-row/numpad/Num-Lock-off combinations hold and release',shortcuts.results.every(r=>r.passed));
  check('First physical key keeps ownership',shortcuts.owner);check('Numpad typing, modifiers and repeat do not start pads',shortcuts.typing&&shortcuts.modified&&shortcuts.repeat);
  check('All eight presets store 4000 Hz and fractional FOLD',shortcuts.presets.every(p=>p.frequencyHz===4000&&p.foldPct===1.2));
  await page.goto(url);await page.waitForLoadState('load');check('Reload ready',await ready());check('Preset precision survives reload',(await state()).presets.every(p=>p.frequencyHz===4000&&p.foldPct===1.2));
  if(await page.evaluate(()=>document.querySelector('#service-panel').hidden))await page.locator('#maintenance-toggle').click();
  await page.locator('#max-frequency').fill('2000');await page.locator('#setup-form button').click();
  await page.goto(url);await page.waitForLoadState('load');check('Custom-bound reload ready',await ready());check('Saved 2000 Hz ceiling is preserved',(await state()).setup.maxHz===2000);
  if(await page.evaluate(()=>document.querySelector('#service-panel').hidden))await page.locator('#maintenance-toggle').click();
  check('Typed field respects saved ceiling',await page.evaluate(()=>document.querySelector('#frequency-input').max==='2000'));
  await page.locator('#max-frequency').fill('4000');await page.locator('#setup-form button').click();
  await page.locator('#frequency-input').fill('4000');await page.keyPress('Enter');
  const render=await page.evaluate(async()=>{
   const {AudioPreview}=await import('./dsp.js');const c=new OfflineAudioContext(1,24000,48000),p=new AudioPreview();p.build(c);p.enabled=true;
   p.update({parameters:{frequencyHz:4000,levelPct:100,wave:'sine',foldPct:.1,harmonics:'odd'},muted:false});
   const a=(await c.startRendering()).getChannelData(0).subarray(12000);return {rms:Math.sqrt(a.reduce((s,v)=>s+v*v,0)/a.length),peak:a.reduce((s,v)=>Math.max(s,Math.abs(v)),0)};
  });check('4000 Hz renders through real Web Audio without clipping',render.rms>.3&&render.peak<1);
  const hashes={};for(const name of ['app.js','model.js','controls.js','index.html','styles.css']){
   const actual=await page.evaluate(async name=>await(await fetch(`./${name}`)).text(),name);
   hashes[name]=createHash('sha256').update(actual).digest('hex');
   check(`Live source matches local: ${name}`,hashes[name]===createHash('sha256').update(readFileSync(`/Users/0xtx/Documents/GitHub/cp-100/simulator/dist/${name}`)).digest('hex'));
  }
  await page.evaluate(()=>document.querySelector('#frequency-form').scrollIntoView({block:'center'}));
  await saveScreenshot(`${directory}/${label}-desktop.png`);
  await page.setViewportSize(390,844);await page.snapshot();
  check('Mobile has no horizontal overflow',await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  check('Mobile input is usable',await page.evaluate(()=>document.querySelector('#frequency-input').getBoundingClientRect().width>=140));
  await page.evaluate(()=>document.querySelector('#frequency-form').scrollIntoView({block:'center'}));
  await saveScreenshot(`${directory}/${label}-mobile.png`);
  writeFileSync(`${directory}/${label}.json`,JSON.stringify({url,label,passed:true,checks,shortcuts,render,hashes},null,2));
  return {url,label,passed:true,checks:checks.length,render};
 }catch(error){writeFileSync(`${directory}/${label}-failure.json`,JSON.stringify({url,label,checks,error:String(error)},null,2));throw error;}
};
