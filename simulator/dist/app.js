import { STORAGE_KEY, SPEC, LIMITS, WAVES, HARMONICS, initialState, reduce, parsePresetFile, exportPresetFile, CONFIG_KEY, DEFAULT_SETUP, validSetup, inRange, ledState } from './model.js';
import { PadSequencer } from './sequencer.js';
import { padNumberForKey } from './controls.js';
import { AudioPreview } from './dsp.js';
import { SandPreview } from './sand.js';
import { GATHER_KEY, GATHER_LIMITS, GatherPlayer, cleanGather, exampleGather, exportGatherFile, gatherDuration, parseGatherFile } from './gather.js';
const $ = selector => document.querySelector(selector);
const harmonicLabels = {odd:'ODD', even:'EVEN', both:'ODD + EVEN'};
const waveLabels = { sine: 'SINE', sawtooth: 'SAW', triangle: 'TRI' };
let restored, storageProblem = false;
try { const data = localStorage.getItem(STORAGE_KEY) || localStorage.getItem('chladni-08-presets-v3'); if (data) restored = parsePresetFile(data); }
catch { restored = Array(8).fill(null); storageProblem = true; }
let setup;try{setup=JSON.parse(localStorage.getItem(CONFIG_KEY));}catch{}
let state = initialState(restored,setup);
let maintenanceOpen=false,heldSource=null;
let lastWrite = true;
let fine = false, screenMessage = '', screenTimer, messageTimer;
const audio = new AudioPreview();
const sand = new SandPreview($('#sand-canvas'));
let gatherSteps;try{const data=localStorage.getItem(GATHER_KEY);gatherSteps=data?parseGatherFile(data):exampleGather();}catch{gatherSteps=exampleGather();}
let gatherPending=false,gatherRequest=0;
const gather=new GatherPlayer({
 onFrame(frame){heldSource=null;dispatch({type:'GATHER_FRAME',frequencyHz:frame.frequencyHz,levelPct:frame.levelPct});},
 onDone(){dispatch({type:'RELEASE_ALL'});notify('GATHER SELESAI / MUTE');}
});
function stopGather(){
 const playing=gather.running||gatherPending;
 gatherRequest++;gatherPending=false;gather.stop();
 if(playing){heldSource=null;dispatch({type:'RELEASE_ALL'});}
}
let sequencePending=false,sequenceRequest=0;
const sequence=new PadSequencer({onStep(index){
 heldSource=null;dispatch({type:'SEQUENCE_STEP',index});
}});
function stopSequence(){
 const playing=sequence.running||sequencePending;
 sequenceRequest++;sequencePending=false;sequence.stop();
 if(playing){heldSource=null;dispatch({type:'RELEASE_ALL'});}
}
$('#sequence-play').addEventListener('click',async()=>{
 if(sequence.running||sequencePending){stopSequence();return;}
 stopGather();const request=++sequenceRequest;sequencePending=true;render();
 try{
  if(!audio.enabled)await audio.enable();
  if(request!==sequenceRequest||document.hidden)return;
  sequencePending=false;heldSource=null;
  sequence.start($('#sequence-order').value);
 }catch(error){
  if(request===sequenceRequest){sequencePending=false;audio.disable();dispatch({type:'MUTE',value:true});notify(error.message);}
 }finally{if(request===sequenceRequest)sequencePending=false;render();}
});
$('#sequence-order').addEventListener('change',()=>{stopSequence();render();});
const fonts = {
'0':['01110','10001','10011','10101','11001','10001','01110'],
'1':['00100','01100','00100','00100','00100','00100','01110'],
'2':['01110','10001','00001','00010','00100','01000','11111'],
'3':['11110','00001','00001','01110','00001','00001','11110'],
'4':['00010','00110','01010','10010','11111','00010','00010'],
'5':['11111','10000','10000','11110','00001','00001','11110'],
'6':['00110','01000','10000','11110','10001','10001','01110'],
'7':['11111','00001','00010','00100','01000','01000','01000'],
'8':['01110','10001','10001','01110','10001','10001','01110'],
'9':['01110','10001','10001','01111','00001','00010','01100'],
'A':['01110','10001','10001','11111','10001','10001','10001'],
'D':['11110','10001','10001','10001','10001','10001','11110'],
'E':['11111','10000','10000','11110','10000','10000','11111'],
'F':['11111','10000','10000','11110','10000','10000','10000'],
'I':['01110','00100','00100','00100','00100','00100','01110'],
'L':['10000','10000','10000','10000','10000','10000','11111'],
'M':['10001','11011','10101','10101','10001','10001','10001'],
'N':['10001','11001','10101','10011','10001','10001','10001'],
'O':['01110','10001','10001','10001','10001','10001','01110'],
'P':['11110','10001','10001','11110','10000','10000','10000'],
'R':['11110','10001','10001','11110','10100','10010','10001'],
'S':['01111','10000','10000','01110','00001','00001','11110'],
'T':['11111','00100','00100','00100','00100','00100','00100'],
'U':['10001','10001','10001','10001','10001','10001','01110'],
'V':['10001','10001','10001','10001','10001','01010','00100'],
'W':['10001','10001','10001','10101','10101','11011','10001'],
'Y':['10001','10001','01010','00100','00100','00100','00100'],
'.':['0','0','0','0','0','0','1'],
'+':['00000','00100','00100','11111','00100','00100','00000'],
' ':['00','00','00','00','00','00','00'],
};
function notify(text) {
  $('#message').textContent = text; clearTimeout(messageTimer);
  messageTimer = setTimeout(() => $('#message').textContent = 'DRAG / SCROLL UNTUK MEMUTAR', 3600);
}
function flash(text) {
  screenMessage = text; clearTimeout(screenTimer); drawMatrix();
  screenTimer = setTimeout(() => { screenMessage = ''; drawMatrix(); }, 900);
}
function persist() {
  try { localStorage.setItem(STORAGE_KEY, JSON.stringify(exportPresetFile(state.presets))); return true; }
  catch { notify('PENYIMPANAN BROWSER TIDAK TERSEDIA'); return false; }
}
function dispatch(action) {
  if((gather.running||gatherPending)&&['MODE','SETUP','PARAMETERS','ARM','MUTE','IMPORT'].includes(action.type)){
    if(action.type==='MUTE')action={...action,value:true};
    stopGather();
  }
  if((sequence.running||sequencePending)&&['MODE','SETUP','PARAMETERS','ARM','MUTE','IMPORT'].includes(action.type)){
    // Stop already mutes; a TEST/MUTE toggle must not turn output back on afterward.
    if(action.type==='MUTE')action={...action,value:true};
    stopSequence();
  }
  const before = state; state = reduce(state, action);
  if (state.presets !== before.presets) lastWrite = persist();
  if(state.setup!==before.setup){try{localStorage.setItem(CONFIG_KEY,JSON.stringify(state.setup));}catch{notify('SETUP HANYA TERSIMPAN SEMENTARA');}}
  if (state !== before) audio.update(state);
  render();
}
function drawMatrix() {
  const canvas = $('#matrix'), ratio = Math.min(devicePixelRatio || 1, 2);
  const width = canvas.clientWidth, height = width / 4;
  if (!width) return;
  canvas.width = Math.round(width * ratio); canvas.height = Math.round(height * ratio);
  const ctx = canvas.getContext('2d'); ctx.scale(ratio, ratio);
  const p = state.parameters;
  const value = screenMessage || p.frequencyHz.toFixed(1);
  const glyphs = [...value].map(char => fonts[char] || fonts[' ']);
  let column = Math.floor((32 - glyphs.reduce((n, glyph) => n + glyph[0].length + 1, -1)) / 2);
  const lit = new Set();
  glyphs.forEach(glyph => { glyph.forEach((row, y) => [...row].forEach((pixel, x) => { if (pixel === '1') lit.add(`${column+x}:${y}`); })); column += glyph[0].length + 1; });
  const pitch = width / 32;
  for (let y = 0; y < 8; y++) for (let x = 0; x < 32; x++) {
    ctx.beginPath(); ctx.arc((x+.5)*pitch,(y+.5)*pitch,pitch*.28,0,Math.PI*2);
    ctx.fillStyle = lit.has(`${x}:${y}`) ? '#d5a262' : '#242424'; ctx.fill();
  }
  canvas.setAttribute('aria-label', screenMessage || `${p.frequencyHz.toFixed(1)} Hz`);
  $('.display-unit').style.visibility = screenMessage ? 'hidden' : 'visible';
}
function pressPad(index,source){
  stopSequence();stopGather();
  if(heldSource)return;
  if(!state.armed&&(!state.presets[index]||!inRange(state.presets[index],state.setup))){
    $('#visitor-status').textContent=state.presets[index]?'Preset di luar batas sweep':'Pad belum diisi';return;
  }
  heldSource=source;const saving=state.armed&&state.mode==='SERVICE';
  dispatch({type:'PAD_DOWN',index});
  if(saving){flash(lastWrite?'SAVED':'RAM');notify(lastWrite?`PRESET DISIMPAN KE PAD ${index+1}`:'PRESET SEMENTARA');}
}
function releasePad(index,source){if(heldSource!==source)return;heldSource=null;dispatch({type:'RELEASE_ALL'});}
function releaseAll(){stopSequence();stopGather();heldSource=null;dispatch({type:'RELEASE_ALL'});}
for(let i=0;i<8;i++){
 const b=document.createElement('button');b.className='pad';b.dataset.pad=i;
 b.setAttribute('aria-keyshortcuts',String(i+1));
 b.innerHTML=`${String(i+1).padStart(2,'0')}<i class="led" aria-hidden="true"></i>`;
 b.addEventListener('pointerdown',event=>{
  if(event.button!==0)return;event.preventDefault();b.focus();b.setPointerCapture(event.pointerId);pressPad(i,`p${event.pointerId}`);
 });
 for(const type of ['pointerup','pointercancel','lostpointercapture'])b.addEventListener(type,event=>releasePad(i,`p${event.pointerId}`));
 b.addEventListener('keydown',event=>{if([' ','Enter'].includes(event.key)){event.preventDefault();if(!event.repeat)pressPad(i,`b${i}`);}});
 b.addEventListener('keyup',event=>{if([' ','Enter'].includes(event.key)){event.preventDefault();releasePad(i,`b${i}`);}});
 b.addEventListener('blur',()=>{if(heldSource===`b${i}`)releasePad(i,`b${i}`);});
 // Assistive technologies can activate a button without a down/up sequence.
 b.addEventListener('click',event=>{if(event.detail===0&&!heldSource){pressPad(i,`a${i}`);releasePad(i,`a${i}`);}});
 $('#pad-grid').append(b);
}
$('#maintenance-toggle').addEventListener('click',()=>{
 maintenanceOpen=!maintenanceOpen;heldSource=null;
 $('#service-panel').hidden=!maintenanceOpen;
 $('#maintenance-toggle').setAttribute('aria-expanded',String(maintenanceOpen));
 $('#maintenance-toggle').textContent=maintenanceOpen?'TUTUP PANEL SERVIS':'PANEL SERVIS';
 dispatch({type:'MODE',mode:maintenanceOpen?'SERVICE':'RUN'});
 if(maintenanceOpen)syncSetupForm();
});
function syncSetupForm(){
 $('#min-frequency').value=state.setup.minHz;$('#max-frequency').value=state.setup.maxHz;
 $('#calibrated').checked=state.setup.calibrated;$('#led-color').value=state.setup.ledColor;$('#led-pulse').checked=state.setup.ledPulse;
}
$('#setup-form').addEventListener('submit',event=>{
 event.preventDefault();const patch={minHz:Number($('#min-frequency').value),maxHz:Number($('#max-frequency').value),calibrated:$('#calibrated').checked,ledColor:$('#led-color').value,ledPulse:$('#led-pulse').checked};
 if(!validSetup(patch)){$('#setup-feedback').textContent='MIN harus lebih kecil dari MAX (20-4000 Hz).';return;}
 heldSource=null;dispatch({type:'SETUP',patch});$('#setup-feedback').textContent='Setup tersimpan';
});
const gatherFields=[['frequencyHz','Hz',.1],['levelPct','Level %',1],['glideS','Glide detik',.1],['holdS','Hold detik',.1]];
function persistGather(){
 try{localStorage.setItem(GATHER_KEY,JSON.stringify(exportGatherFile(gatherSteps)));return true;}catch{return false;}
}
function buildGatherRows(){
 const body=$('#gather-rows');body.textContent='';
 gatherSteps.forEach((step,i)=>{
  const row=document.createElement('tr');row.dataset.gatherStep=i;
  row.innerHTML=`<th scope="row">${i+1}</th>`+gatherFields.map(([key,label,step2])=>{
   const [min,max]=GATHER_LIMITS[key];
   return `<td><input type="number" inputmode="decimal" data-gather-field="${key}" min="${min}" max="${max}" step="${step2}" value="${step[key]}" aria-label="Langkah ${i+1} ${label}"></td>`;
  }).join('')+`<td><button type="button" class="gather-remove" data-gather-remove="${i}" aria-label="Hapus langkah ${i+1}">✕</button></td>`;
  body.append(row);
 });
}
function editGather(next,rebuild){
 stopGather();
 try{gatherSteps=cleanGather(next);$('#gather-feedback').textContent=persistGather()?'GATHER TERSIMPAN':'GATHER HANYA TERSIMPAN SEMENTARA';}
 catch(error){$('#gather-feedback').textContent=error.message;rebuild=true;}
 if(rebuild)buildGatherRows();render();
}
$('#gather-rows').addEventListener('change',event=>{
 const input=event.target.closest('[data-gather-field]');if(!input)return;
 const i=Number(input.closest('[data-gather-step]').dataset.gatherStep),value=input.valueAsNumber;
 if(!input.checkValidity()||!Number.isFinite(value)){$('#gather-feedback').textContent=`Langkah ${i+1}: nilai di luar batas.`;input.value=gatherSteps[i][input.dataset.gatherField];return;}
 editGather(gatherSteps.map((step,j)=>j===i?{...step,[input.dataset.gatherField]:value}:step),false);
});
$('#gather-rows').addEventListener('click',event=>{
 const button=event.target.closest('[data-gather-remove]');if(!button)return;
 editGather(gatherSteps.filter((_,j)=>j!==Number(button.dataset.gatherRemove)),true);
});
$('#gather-add').addEventListener('click',()=>{
 const p=state.parameters;editGather([...gatherSteps,{frequencyHz:p.frequencyHz,levelPct:p.levelPct,glideS:4,holdS:4}],true);
});
$('#gather-example').addEventListener('click',()=>editGather(exampleGather(),true));
$('#gather-play').addEventListener('click',async()=>{
 if(gather.running||gatherPending){stopGather();return;}
 if(state.mode!=='SERVICE')return;
 stopSequence();const request=++gatherRequest;gatherPending=true;render();
 try{
  if(!audio.enabled)await audio.enable();
  if(request!==gatherRequest||document.hidden)return;
  gatherPending=false;heldSource=null;gather.start(gatherSteps);
 }catch(error){
  if(request===gatherRequest){gatherPending=false;gather.stop();dispatch({type:'RELEASE_ALL'});notify(error.message);}
 }finally{if(request===gatherRequest)gatherPending=false;render();}
});
function renderGather(service){
 const gathering=gather.running||gatherPending,frame=gather.frame,phases={glide:'GLIDE',hold:'HOLD',fade:'FADE'};
 $('#gather-play').textContent=gathering?'■ STOP':'▶ GATHER';
 $('#gather-play').setAttribute('aria-pressed',String(gathering));
 $('#gather-play').disabled=!service;$('#gather-add').disabled=!service||gatherSteps.length>=GATHER_LIMITS.steps[1];$('#gather-example').disabled=!service;
 document.querySelectorAll('#gather-rows input,#gather-rows button').forEach(el=>el.disabled=!service||(el.dataset.gatherRemove!==undefined&&gatherSteps.length<=GATHER_LIMITS.steps[0]));
 const outside=gatherSteps.filter(s=>s.frequencyHz<state.setup.minHz||s.frequencyHz>state.setup.maxHz).length;
 document.querySelectorAll('[data-gather-step]').forEach(row=>{
  const i=Number(row.dataset.gatherStep),s=gatherSteps[i];
  row.classList.toggle('current',Boolean(gather.running&&frame&&frame.index===i));
  row.classList.toggle('outside',Boolean(s&&(s.frequencyHz<state.setup.minHz||s.frequencyHz>state.setup.maxHz)));
 });
 $('#gather-status').textContent=gatherPending?'MENYIAPKAN AUDIO':gather.running&&frame?`LANGKAH ${frame.index+1}/${gatherSteps.length} · ${phases[frame.phase]} · ${state.parameters.frequencyHz.toFixed(1)} Hz · L${state.parameters.levelPct}`:`${gatherSteps.length} LANGKAH · ${gatherDuration(gatherSteps).toFixed(1)} DETIK · SEKALI LALU MUTE${outside?` · ${outside} DI LUAR BATAS SWEEP`:''}`;
}
function render() {
  const p=state.parameters, service=state.mode==='SERVICE';
  const sequencing=sequence.running||sequencePending;
  $('#sequence-play').textContent=sequencing?'■ STOP':'▶ PLAY';
  $('#sequence-play').setAttribute('aria-pressed',String(sequencing));
  $('#sequence-play').setAttribute('aria-label',sequencing?'Hentikan sequence':'Putar sequence');
  const sequencePreset=sequence.index===null?null:state.presets[sequence.index];
  $('#sequence-status').textContent=sequencePending?'MENYIAPKAN AUDIO':sequence.running?`PAD ${sequence.index+1} / 3 DETIK${!sequencePreset?' · KOSONG':!inRange(sequencePreset,state.setup)?' · DI LUAR BATAS':''}`:'3 DETIK / PAD · ULANG';
  $('.device').classList.toggle('locked',!service);
  document.querySelectorAll('[data-mode]').forEach(button=>button.setAttribute('aria-pressed',String(button.dataset.mode===state.mode)));
  $('#save-action').disabled=!service;$('#save-action').setAttribute('aria-pressed',String(state.armed));
  $('#mute-action').setAttribute('aria-pressed',String(state.muted));
  $('#audio-toggle').setAttribute('aria-pressed',String(audio.enabled));
  $('#audio-label').textContent=audio.enabled?'AUDIO AKTIF':'NYALAKAN AUDIO';
  $('#audio-toggle').setAttribute('aria-label',audio.enabled?'Matikan audio':'Nyalakan audio');
  $('#audio-toggle').title=audio.enabled?'Klik untuk mematikan suara':'Klik untuk menyalakan suara';
  $('#precision-toggle').textContent=fine?'0.1 Hz':'1 Hz';
  const frequencyInput=$('#frequency-input');
  frequencyInput.min=state.setup.minHz;frequencyInput.max=state.setup.maxHz;
  if(document.activeElement!==frequencyInput)frequencyInput.value=p.frequencyHz.toFixed(1);
  $('#frequency-range').textContent=`${state.setup.minHz}–${state.setup.maxHz} Hz`;
  document.querySelectorAll('#frequency-form input,#frequency-form button').forEach(el=>el.disabled=!service);
  $('#level-value').textContent=`${p.levelPct}%`;$('#fold-value').textContent=`${p.foldPct.toFixed(1)}%`;
  document.querySelectorAll('[data-choice]').forEach(button=>{button.setAttribute('aria-pressed',String(p[button.dataset.choice]===button.dataset.value));button.disabled=!service;});
  document.querySelectorAll('.control button').forEach(b=>b.disabled=!service);
  document.querySelectorAll('[data-knob]').forEach(k=>{
    const key=k.dataset.knob,choices=key==='wave'?WAVES:key==='harmonics'?HARMONICS:null,value=choices?choices.indexOf(p[key]):p[key];
    const [min,max]=choices?[0,2]:key==='frequencyHz'?[state.setup.minHz,state.setup.maxHz]:LIMITS[key];
    k.setAttribute('aria-valuemin',String(min));k.setAttribute('aria-valuemax',String(max));
    k.style.setProperty('--angle',`${-135+270*(value-min)/(max-min)}deg`);
    k.setAttribute('aria-valuenow',String(value));
    k.setAttribute('aria-disabled',String(!service));
    k.setAttribute('aria-valuetext',key==='wave'?waveLabels[p.wave]:key==='harmonics'?harmonicLabels[p.harmonics]:`${value}${key==='frequencyHz'?' Hz':'%'}`);
  });
  const playing=audio.enabled&&!state.muted&&p.levelPct>0;
  $('#output-status').textContent=state.armed?'SAVE → PAD':playing?state.activePad===null?'PLAY':`PAD ${String(state.activePad+1).padStart(2,'0')}`:state.muted?'MUTE':'AUDIO OFF';
  $('#output-status').classList.toggle('playing',playing);
  document.querySelectorAll('[data-pad]').forEach(b=>{
    const i=Number(b.dataset.pad),preset=state.presets[i];
    const led=ledState(state,i),blocked=preset&&!inRange(preset,state.setup);
    b.classList.toggle('sequenced',sequence.running&&sequence.index===i);
    b.classList.toggle('active',led.active);b.classList.toggle('empty',!preset||blocked);b.classList.toggle('armed',state.armed);
    b.dataset.color=led.color;b.classList.toggle('pulsing',led.pulseHz>0);b.style.setProperty('--pulse-duration',`${1/(led.pulseHz||1)}s`);
    b.setAttribute('aria-disabled',String((!preset||blocked)&&!state.armed));
    b.setAttribute('aria-pressed',String(state.activePad===i));
    b.setAttribute('aria-label',`Pad ${i+1}, tekan dan tahan, ${blocked?'di luar batas sweep':preset?`${preset.frequencyHz} Hz, ${waveLabels[preset.wave]}, fold ${preset.foldPct}%, ${harmonicLabels[preset.harmonics]}, level ${preset.levelPct}%`:'kosong'}`);
    b.title=(preset?`${preset.frequencyHz} Hz · ${waveLabels[preset.wave]} · F${preset.foldPct} · L${preset.levelPct}`:'Belum disimpan')+` · Keyboard ${i+1} / Numpad ${i+1}`;
  });
  $('#visitor-status').textContent=gather.running?'GATHER AKTIF / STOP DI PANEL SERVIS':sequence.running?'SEQUENCE AKTIF / STOP UNTUK BERHENTI':state.activePad!==null?`PAD ${String(state.activePad+1).padStart(2,'0')} / LEPAS UNTUK BERHENTI`:'Tahan pad / tombol 1–8 · lepas untuk berhenti';
  $('#calibration-status').textContent=state.setup.calibrated?'BATAS TERSIMPAN':'BELUM DIKALIBRASI';
  document.querySelectorAll('#setup-form input,#setup-form select,#setup-form button').forEach(el=>el.disabled=!service);
  $('#sand-frequency').textContent=`${p.frequencyHz.toFixed(1)} Hz`;
  $('#sand-wave').textContent=`${waveLabels[p.wave]} / ${harmonicLabels[p.harmonics]}`;
  $('#sand-status').textContent=state.muted||p.levelPct===0?'MUTE':'ACTIVE';
  $('#sand-status').classList.toggle('active',!state.muted&&p.levelPct>0);
  renderGather(service);
  sand.setState(state);
  drawMatrix();
}
function setParameter(key,value) {
  if(state.mode!=='SERVICE')return;
  dispatch({type:'PARAMETERS',patch:{[key]:value}});
  if(key==='wave') flash(waveLabels[value]);
  else if(key==='harmonics')flash(value==='both'?'O+E':harmonicLabels[value]);
  else if(key==='foldPct') flash(`F${state.parameters.foldPct.toFixed(1)}`);
  else if(key==='levelPct') flash(`L ${String(value).padStart(3,'0')}`);
  else {screenMessage='';clearTimeout(screenTimer);drawMatrix();}
}
function toggleFine(){if(state.mode!=='SERVICE')return;fine=!fine;render();notify(`LANGKAH ${fine?'0.1':'1'} Hz`);}
function nextWave(){if(state.mode==='SERVICE')setParameter('wave',WAVES[(WAVES.indexOf(state.parameters.wave)+1)%3]);}
function nextHarmonics(){if(state.mode==='SERVICE')setParameter('harmonics',HARMONICS[(HARMONICS.indexOf(state.parameters.harmonics)+1)%3]);}
$('#frequency-form').addEventListener('submit',event=>{
 event.preventDefault();const input=$('#frequency-input');
 if(!input.reportValidity())return;
 setParameter('frequencyHz',input.valueAsNumber);input.value=state.parameters.frequencyHz.toFixed(1);
});
$('#precision-toggle').addEventListener('click',toggleFine);
document.querySelectorAll('[data-choice]').forEach(button=>button.addEventListener('click',()=>setParameter(button.dataset.choice,button.dataset.value)));
document.querySelectorAll('[data-mode]').forEach(button=>button.addEventListener('click',()=>{if(button.dataset.mode===state.mode)return;heldSource=null;if(button.dataset.mode==='RUN'){maintenanceOpen=false;$('#service-panel').hidden=true;$('#maintenance-toggle').setAttribute('aria-expanded','false');$('#maintenance-toggle').textContent='PANEL SERVIS';}dispatch({type:'MODE',mode:button.dataset.mode});notify(state.mode==='SERVICE'?'SERVICE / KNOB AKTIF':'RUN / KNOB TERKUNCI');}));
$('#save-action').addEventListener('click',()=>{dispatch({type:'ARM'});flash(state.armed?'SAVE':state.parameters.frequencyHz.toFixed(1));notify(state.armed?'TEKAN PAD TUJUAN / SAVE LAGI UNTUK BATAL':'SAVE BATAL');});
$('#mute-action').addEventListener('click',()=>{dispatch({type:'MUTE'});notify(state.muted?'MUTE':audio.enabled?'PLAY':'AKTIFKAN AUDIO');});
$('#audio-toggle').addEventListener('click',async()=>{
  const button=$('#audio-toggle');button.disabled=true;
  try{if(audio.enabled){audio.disable();dispatch({type:'MUTE',value:true});notify('AUDIO OFF');}else{await audio.enable();audio.update(state);notify('AUDIO SIAP / TEKAN PAD');}}
  catch(error){audio.disable();notify(error.message);}
  finally{button.disabled=false;render();}
});
function choicesFor(key){return key==='wave'?WAVES:key==='harmonics'?HARMONICS:null;}
function knobValue(key){const choices=choicesFor(key);return choices?choices.indexOf(state.parameters[key]):state.parameters[key];}
function increment(key,delta,start=knobValue(key)){
  if(state.mode!=='SERVICE')return;
  const choices=choicesFor(key),[min,max]=choices?[0,2]:key==='frequencyHz'?[state.setup.minHz,state.setup.maxHz]:LIMITS[key];
  const value=Math.max(min,Math.min(max,start+delta));
  setParameter(key,choices?choices[Math.round(value)]:value);
}
document.querySelectorAll('[data-knob]').forEach(knob=>{
  const key=knob.dataset.knob, selector=Boolean(choicesFor(key));
  const ticks=Array.from({length:selector?3:21},(_,i)=>{const angle=(-135+270*i/(selector?2:20))*Math.PI/180;return `<line x1="${50+43*Math.sin(angle)}" y1="${50-43*Math.cos(angle)}" x2="${50+47*Math.sin(angle)}" y2="${50-47*Math.cos(angle)}"/>`;}).join('');
  knob.insertAdjacentHTML('afterbegin',`<svg viewBox="0 0 100 100" aria-hidden="true">${ticks}</svg>`);
  let drag=null,suppressClick=false;
  knob.addEventListener('pointerdown',event=>{
    if(state.mode!=='SERVICE')return;event.preventDefault();knob.focus();knob.setPointerCapture(event.pointerId);
    suppressClick=false;drag={y:event.clientY,start:knobValue(key),moved:false};
  });
  knob.addEventListener('pointermove',event=>{
    if(!drag)return;const distance=drag.y-event.clientY;if(Math.abs(distance)>3)drag.moved=true;
    if(!drag.moved)return;const step=key==='frequencyHz'?(fine?.1:1):key==='foldPct'?.1:1;
    increment(key,Math.round(distance/(selector?22:2))*step,drag.start);
  });
  const end=()=>{if(drag){suppressClick=drag.moved;drag=null;}};
  knob.addEventListener('pointerup',end);knob.addEventListener('pointercancel',()=>{suppressClick=true;drag=null;});knob.addEventListener('lostpointercapture',end);
  function push(){if(key==='frequencyHz')toggleFine();if(key==='wave')nextWave();if(key==='harmonics')nextHarmonics();}
  knob.addEventListener('click',()=>{if(suppressClick){suppressClick=false;return;}push();});
  knob.addEventListener('wheel',event=>{if(state.mode!=='SERVICE')return;event.preventDefault();increment(key,(event.deltaY<0?1:-1)*(key==='foldPct'||key==='frequencyHz'&&fine?.1:1));},{passive:false});
  knob.addEventListener('keydown',event=>{
    if(state.mode!=='SERVICE')return;
    if(['ArrowUp','ArrowRight','ArrowDown','ArrowLeft','PageUp','PageDown'].includes(event.key)){
      event.preventDefault();const positive=['ArrowUp','ArrowRight','PageUp'].includes(event.key);increment(key,(positive?1:-1)*(key==='foldPct'||key==='frequencyHz'&&fine?.1:1)*(event.key.startsWith('Page')?10:1));
    }else if(event.key==='Home'||event.key==='End'){
      event.preventDefault();const choices=choicesFor(key),bounds=choices?[0,2]:key==='frequencyHz'?[state.setup.minHz,state.setup.maxHz]:LIMITS[key],index=event.key==='Home'?0:1;setParameter(key,choices?choices[bounds[index]]:bounds[index]);
    }else if(event.key==='Enter'||event.key===' '){event.preventDefault();push();}
  });
});
document.addEventListener('keydown',event=>{
  if(event.ctrlKey||event.metaKey||event.altKey||event.target.closest('[role=slider]')||['INPUT','TEXTAREA','SELECT'].includes(event.target.tagName))return;
  const number=padNumberForKey(event);
  if(number!==null){event.preventDefault();if(!event.repeat)pressPad(number-1,`k${event.code||event.key}`);}
  if(event.code==='Space'&&event.target.tagName!=='BUTTON'){event.preventDefault();dispatch({type:'MUTE',value:true});}
});
document.addEventListener('keyup',event=>{const number=padNumberForKey(event);if(number!==null)releasePad(number-1,`k${event.code||event.key}`);});
window.addEventListener('blur',releaseAll);
window.addEventListener('pagehide',releaseAll);
document.addEventListener('visibilitychange',()=>{if(document.hidden)releaseAll();});
window.addEventListener('resize',drawMatrix);
window.chladniDiagnostics=()=>({state:structuredClone(state),audio:audio.diagnostics(),sequence:{...sequence.diagnostics(),pending:sequencePending},gather:{...gather.diagnostics(),pending:gatherPending},spec:SPEC,display:$('#matrix').getAttribute('aria-label'),sand:sand.diagnostics()});
buildGatherRows();syncSetupForm();render();if(storageProblem)notify('PRESET TIDAK TERBACA / SLOT DIKOSONGKAN');
