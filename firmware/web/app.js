'use strict';
const $ = id => document.getElementById(id);
const keys = ['frequencyHz', 'levelPct', 'foldPct'];
const limits = { frequencyHz: [20, 4000, .1], levelPct: [0, 100, 1], foldPct: [0, 100, .1] };
const examples = [
  { frequencyHz:55.6,levelPct:40,glideS:2,holdS:4 },{ frequencyHz:88.2,levelPct:36,glideS:4,holdS:4 },
  { frequencyHz:140,levelPct:32,glideS:4,holdS:4 },{ frequencyHz:222.2,levelPct:26,glideS:4,holdS:4 },
  { frequencyHz:352.8,levelPct:20,glideS:4,holdS:4 }
];
let heldPointer = null, heldKey = null;
let state, parameters, token = '', heldPad = -1, queue = Promise.resolve(), generation = 0;
let parameterVersion = 0, parameterWrites = 0, parameterFlight = null;
let parameterTimer, parameterDirty = false, setupDirty = false, gatherDirty = false, dragging = null;
let steps = [], lastSetup = '', lastGather = '', polling = false, beating = false;
function notice(text, error = false) { $('notice').textContent = text; $('notice').classList.toggle('error', error); }
async function request(path, payload, session = token, keepalive = false) {
  const controller = new AbortController(), timeout = setTimeout(() => controller.abort(), 1500);
  try {
    const options = { cache:'no-store', signal:controller.signal, keepalive, headers:{'X-CP100-Client':'1'} };
    if (payload !== undefined) { options.method = 'POST'; options.headers['Content-Type'] = 'application/json'; if (payload !== null) options.body = JSON.stringify(payload); }
    if (session) options.headers['X-CP100-Session'] = session;
    const response = await fetch(path, options);
    const data = await response.json();
    if (!response.ok || data.ok === false) { const error = new Error(data.message || 'Perintah ditolak.'); error.status = response.status; throw error; }
    return data;
  } finally { clearTimeout(timeout); }
}
function invalidate() {
  generation++; clearTimeout(parameterTimer); parameterTimer = null; parameterDirty = false; heldPad = -1; heldPointer = null; heldKey = null; dragging = null;
}
function offline(message) {
  invalidate(); token = ''; $('connection').textContent = 'KONTROL TERPUTUS'; $('connection').classList.remove('online');
  $('connectPanel').hidden = false; $('workspace').hidden = true; $('stopDock').hidden = true; $('connect').disabled = false;
  $('connectNote').textContent = message + ' Output servis akan mute otomatis. Sambungkan kembali untuk melanjutkan.';
}
function send(payload) {
  notice('');
  const epoch = generation, session = token;
  const task = queue.then(async () => {
    if (!session || epoch !== generation || session !== token) return false;
    try {
      const data = await request('/api/action', payload, session);
      if (epoch !== generation || session !== token) return false;
      render(data.state);
      if (data.persisted === false) notice('Gagal menyimpan ke perangkat. Perubahan hanya ada di RAM sampai listrik mati.', true);
      return data;
    } catch (error) {
      if (epoch === generation && session === token) {
        if (error.status === 403 || !error.status) offline(error.message);
        else notice(error.message, true);
      }
      return false;
    }
  });
  queue = task.catch(() => false); return task;
}
function renderParameters() {
  if (!parameters) return;
  for (const key of keys) {
    const [lo, hi] = key === 'frequencyHz' && state ? [state.setup.minHz, state.setup.maxHz] : limits[key];
    const input = $(key), knob = document.querySelector(`.knob[data-key="${key}"]`), value = parameters[key];
    if (document.activeElement !== input) input.value = key === 'levelPct' ? value : value.toFixed(1);
    input.min = lo; input.max = hi;
    const fraction = Math.max(0, Math.min(1, (value - lo) / (hi - lo)));
    knob.style.setProperty('--angle', `${-135 + fraction * 270}deg`); knob.style.setProperty('--fill', `${fraction * 270}deg`);
    knob.setAttribute('aria-valuenow', value); knob.setAttribute('aria-valuemin', lo); knob.setAttribute('aria-valuemax', hi);
  }
  for (const key of ['wave','harmonics']) if (document.activeElement !== $(key)) $(key).value = parameters[key];
}
function render(next) {
  if (state && ((next.uptimeMs - state.uptimeMs) | 0) < 0) return;
  state = next;
  if (!parameterDirty && !dragging && !parameterWrites) parameters = {...next.parameters};
  $('frequencyReadout').textContent = next.parameters.frequencyHz.toFixed(1);
  $('outputState').textContent = next.muted ? 'MUTE' : 'OUTPUT AKTIF'; $('outputState').classList.toggle('sounding', !next.muted);
  $('sweepReadout').textContent = `${next.setup.minHz}–${next.setup.maxHz} Hz`;
  const playback = next.playback;
  $('playbackState').textContent = playback.kind === 'idle' ? 'SIAGA' : playback.kind === 'gather' ? `GATHER / LANGKAH ${playback.step + 1}` : `SEQUENCE / PAD ${playback.step + 1}`;
  document.querySelectorAll('[data-mode]').forEach(button => button.classList.toggle('selected', button.dataset.mode === next.mode));
  $('serviceControls').disabled = next.mode !== 'service'; $('test').disabled = next.mode !== 'service';
  document.querySelectorAll('.knob').forEach(knob => { knob.setAttribute('aria-disabled', next.mode !== 'service'); knob.tabIndex = next.mode === 'service' ? 0 : -1; });
  for (let i = 0; i < 4; i++) {
    const pad = $(`pad${i}`); pad.classList.toggle('active', next.activePad === i && !next.muted);
    pad.querySelector('small').textContent = next.presets[i] ? `${next.presets[i].frequencyHz.toFixed(1)} Hz` : 'KOSONG';
  }
  renderParameters();
  const setup = JSON.stringify(next.setup);
  if (!setupDirty && setup !== lastSetup) {
    lastSetup = setup;
    for (const key of ['minHz','maxHz']) $(key).value = next.setup[key];
  }
  const gather = JSON.stringify(next.gather);
  if (!gatherDirty && gather !== lastGather) { lastGather = gather; steps = structuredClone(next.gather); renderGather(); }
  $('diagnostics').textContent = `ESP32 / 48 kHz · AUDIO ${Math.round(next.audio.loadPct)}% · STALL ${next.audio.stalls}`;
}
function editParameter(key, value) {
  if (!parameters || !token || state.mode !== 'service') return;
  let [lo, hi, step] = limits[key];
  if (key === 'frequencyHz') [lo, hi] = [state.setup.minHz, state.setup.maxHz];
  const number = Number(value);
  if (!Number.isFinite(number)) return;
  parameters[key] = Math.round(Math.max(lo, Math.min(hi, number)) / step) * step;
  parameters[key] = Number(parameters[key].toFixed(1));
  renderParameters(); parameterVersion++; parameterDirty = true;
  if (!parameterTimer && !parameterFlight) parameterTimer = setTimeout(flushParameters, 60);
}
async function flushParameters() {
  clearTimeout(parameterTimer); parameterTimer = null;
  const epoch = generation;
  if (parameterFlight) {
    const previous = await parameterFlight;
    if (!previous || epoch !== generation) return false;
    return flushParameters();
  }
  if (!parameterDirty) return true;
  const payload = {...parameters}, version = parameterVersion;
  parameterDirty = false; parameterWrites++;
  const flight = send({action:'parameters', parameters:payload}); parameterFlight = flight;
  const data = await flight;
  parameterWrites--; parameterFlight = null;
  if (epoch !== generation) {
    if (parameterDirty && token && !parameterTimer) parameterTimer = setTimeout(flushParameters,60);
    return false;
  }
  if (version === parameterVersion && !parameterDirty && data) { parameters = {...data.state.parameters}; renderParameters(); }
  // At most one adjustment request in flight; send the newest gesture value next.
  if (data && parameterDirty) return flushParameters();
  return !!data;
}
async function afterParameters(payload) { const epoch = generation; if (await flushParameters() && epoch === generation && token) return send(payload); return false; }
function stop() {
  invalidate(); send({action:'mute'}); notice('Output dihentikan.');
}
function releasePad() {
  if (heldPad < 0) return;
  const index = heldPad; heldPad = -1; heldPointer = null; heldKey = null; send({action:'padUp', index});
}
async function pressPad(index) {
  if (heldPad >= 0 || !token || state.mode !== 'service') return;
  heldPad = index;
  if (!await flushParameters()) { heldPad = -1; return; }
  // A release may have happened while the last edit was in flight.
  if (heldPad !== index) return;
  const result = await send({action:'padDown',index});
  if (!result && heldPad === index) heldPad = -1;
}
function renderGather() {
  $('gatherSteps').replaceChildren();
  const fields = [['frequencyHz',20,4000,.1,'Hz'],['levelPct',0,100,1,'Level'],['glideS',0,60,.1,'Glide'],['holdS',0,60,.1,'Hold']];
  steps.forEach((step,index) => {
    const row = document.createElement('div'); row.className = 'gather-step';
    for (const [key,min,max,increment,label] of fields) {
      const input = document.createElement('input'); input.type = 'number'; input.min = min; input.max = max; input.step = increment; input.inputMode = 'decimal'; input.value = Number(step[key].toFixed(1));
      input.setAttribute('aria-label',`Langkah ${index+1} ${label}`);
      input.addEventListener('input', () => { step[key] = input.value === '' ? NaN : Number(input.value); gatherDirty = true; renderDuration(); });
      row.append(input);
    }
    const remove = document.createElement('button'); remove.textContent = '×'; remove.setAttribute('aria-label',`Hapus langkah ${index+1}`); remove.disabled = steps.length === 1;
    remove.addEventListener('click',() => { steps.splice(index,1); gatherDirty = true; renderGather(); }); row.append(remove);
    $('gatherSteps').append(row);
  });
  $('addStep').disabled = steps.length >= 16; renderDuration();
}
function renderDuration() {
  const duration = steps.reduce((sum,step) => sum + step.glideS + step.holdS,2);
  $('gatherDuration').textContent = Number.isFinite(duration) ? `${duration.toFixed(1)} / 300 detik` : 'Lengkapi angka langkah';
}
function validSteps() {
  const ok = steps.length >= 1 && steps.length <= 16 && steps.every(s =>
    Number.isFinite(s.frequencyHz) && s.frequencyHz >= 20 && s.frequencyHz <= 4000 && Number.isFinite(s.levelPct) && s.levelPct >= 0 && s.levelPct <= 100 &&
    Number.isFinite(s.glideS) && s.glideS >= 0 && s.glideS <= 60 && Number.isFinite(s.holdS) && s.holdS >= 0 && s.holdS <= 60) &&
    steps.reduce((sum,s)=>sum+s.glideS+s.holdS,2) <= 300;
  if (!ok) notice('Lengkapi GATHER: 1–16 langkah, angka dalam batas, maksimal 300 detik.',true);
  return ok;
}
async function saveGather() {
  if (!validSteps()) return false;
  const data = await afterParameters({action:'gatherConfigure',steps:structuredClone(steps)});
  if (data) { gatherDirty = false; lastGather = ''; render(data.state); if (data.persisted) notice('Rutin GATHER tersimpan di perangkat.'); }
  return data;
}
$('connect').addEventListener('click',async () => {
  $('connect').disabled = true;
  try {
    const data = await request('/api/session',null,'');
    invalidate(); state = undefined; token = data.token; parameters = {...data.state.parameters}; setupDirty = gatherDirty = false; lastSetup = lastGather = '';
    $('workspace').hidden = false; $('connectPanel').hidden = true; $('stopDock').hidden = false;
    $('connection').textContent = 'TERHUBUNG'; $('connection').classList.add('online');
    render(data.state); notice('SERVICE aktif. Output masih mute.');
  } catch (error) { $('connectNote').textContent = error.message; }
  finally { $('connect').disabled = false; }
});
for (const key of keys) {
  $(key).addEventListener('input',() => {
    if ($(key).value !== '' && $(key).validity.valid) editParameter(key,$(key).value);
  });
  $(key).addEventListener('change',() => {
    if ($(key).value === '' || !$(key).validity.valid) notice('Angka di luar batas yang diizinkan.',true);
  });
}
for (const key of ['wave','harmonics']) $(key).addEventListener('change',() => {
  parameters[key] = $(key).value; parameterVersion++; parameterDirty = true; flushParameters();
});
document.querySelectorAll('[data-nudge]').forEach(button => button.addEventListener('click',() => {
  const [key,delta] = button.dataset.nudge.split(':'); editParameter(key,parameters[key]+Number(delta));
}));
document.querySelectorAll('.knob').forEach(knob => {
  knob.addEventListener('pointerdown',event => {
    if (!token || state.mode !== 'service' || dragging) return;
    const key = knob.dataset.key; dragging = {key,start:parameters[key],y:event.clientY,id:event.pointerId}; knob.setPointerCapture(event.pointerId); event.preventDefault();
  });
  knob.addEventListener('pointermove',event => {
    if (!dragging || dragging.id !== event.pointerId) return;
    const key = dragging.key, scale = key === 'frequencyHz' ? (event.shiftKey ? .1 : 2) : key === 'foldPct' ? .2 : .5;
    editParameter(key,dragging.start+(dragging.y-event.clientY)*scale);
  });
  const finish = () => { if (dragging) { dragging = null; flushParameters(); } };
  knob.addEventListener('pointerup',finish); knob.addEventListener('pointercancel',finish);
  knob.addEventListener('keydown',event => {
    const direction = ['ArrowUp','ArrowRight'].includes(event.key) ? 1 : ['ArrowDown','ArrowLeft'].includes(event.key) ? -1 : 0;
    if (direction) { event.preventDefault(); const key = knob.dataset.key; editParameter(key,parameters[key]+direction*limits[key][2]*(event.shiftKey?10:1)); }
  });
});
for (let i=0;i<4;i++) {
  const pad = document.createElement('button'); pad.id = `pad${i}`; pad.className = 'pad'; pad.innerHTML = `<strong>0${i+1}</strong><small>—</small>`; pad.setAttribute('aria-label',`Tahan pad ${i+1}`);
  pad.addEventListener('pointerdown',event => { if (heldPad >= 0) return; event.preventDefault(); heldPointer = event.pointerId; heldKey = null; pad.setPointerCapture(event.pointerId); pressPad(i); });
  for (const type of ['pointerup','pointercancel','lostpointercapture']) pad.addEventListener(type,event => { if (heldPad === i && heldPointer === event.pointerId) releasePad(); });
  pad.addEventListener('keydown',event => { if ((event.key===' ' || event.key==='Enter') && !event.repeat && heldPad < 0) { event.preventDefault(); heldKey = event.key; heldPointer = null; pressPad(i); } });
  pad.addEventListener('keyup',event => { if (heldPad === i && heldKey === event.key) { event.preventDefault(); releasePad(); } });
  $('pads').append(pad);
  const save = document.createElement('button'); save.textContent = `SIMPAN ${i+1}`; save.addEventListener('click',async () => { releasePad(); const data = await afterParameters({action:'save',index:i}); if (data?.persisted) notice(`Preset ${i+1} tersimpan di perangkat.`); }); $('saveSlots').append(save);
}
$('test').addEventListener('click',() => { releasePad(); afterParameters({action:'test'}); });
$('mute').addEventListener('click',stop);
$('panic').addEventListener('click',stop);
document.querySelectorAll('[data-mode]').forEach(button => button.addEventListener('click',() => { invalidate(); send({action:'mode',mode:button.dataset.mode}); }));
$('sequence').addEventListener('click',() => { releasePad(); afterParameters({action:'sequence',mode:$('sequenceMode').value}); });
$('addStep').addEventListener('click',() => { if (steps.length<16) { steps.push({frequencyHz:parameters.frequencyHz,levelPct:parameters.levelPct,glideS:4,holdS:4}); gatherDirty = true; renderGather(); } });
$('exampleGather').addEventListener('click',() => { steps=structuredClone(examples);gatherDirty=true;renderGather(); });
$('saveGather').addEventListener('click',saveGather);
$('gather').addEventListener('click',async () => { releasePad(); if (gatherDirty && !await saveGather()) return; afterParameters({action:'gatherStart'}); });
for (const key of ['minHz','maxHz']) $(key).addEventListener('input',() => { setupDirty=true; });
$('saveSetup').addEventListener('click',async () => {
  if (!$('minHz').validity.valid || !$('maxHz').validity.valid || !$('minHz').value || !$('maxHz').value || Number($('minHz').value)>=Number($('maxHz').value)) { notice('SWEEP MIN harus lebih kecil dari MAX, di antara 20–4000 Hz.',true); return; }
  releasePad(); const setup = {minHz:Number($('minHz').value),maxHz:Number($('maxHz').value)};
  const data = await afterParameters({action:'setup',setup});
  if (data) { setupDirty=false;lastSetup='';render(data.state);if(data.persisted)notice('Setup tersimpan. Output mute.'); }
});
$('export').addEventListener('click',() => {
  const file={schema:'chladni-08',version:4,folding:'parity-fold-v2',presets:state.presets};
  const url=URL.createObjectURL(new Blob([JSON.stringify(file,null,2)],{type:'application/json'})); const link=document.createElement('a');link.href=url;link.download='CP-100-4-pad-presets.json';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
});
$('import').addEventListener('click',()=> $('presetFile').click());
$('presetFile').addEventListener('change',async () => {
  const file=$('presetFile').files[0];if(!file)return;
  try {
    if(file.size>4096)throw new Error('File preset maksimal 4096 byte.');
    const payload=JSON.parse(await file.text());
    if(payload.presets?.length===8 && !window.confirm('File lama berisi 8 slot. Gunakan hanya slot 1–4? File asli tetap tersedia.'))return;
    releasePad();const data=await afterParameters({action:'import',file:payload});if(data?.persisted)notice('Empat slot preset berhasil diimpor.');
  }catch(error){notice(error.message,true);}finally{$('presetFile').value='';}
});
$('factory').addEventListener('click',async ()=> {if(window.confirm('Ganti keempat preset tersimpan dengan preset awal 1096 / 1360 / 2249 / 2386 Hz?')) {releasePad();const data=await afterParameters({action:'factory'});if(data?.persisted)notice('Empat preset awal dipulihkan.');}});
function endSession() {
  const session=token;invalidate();token='';
  if(session)request('/api/action',{action:'release'},session,true).catch(()=>{});
}
$('disconnect').addEventListener('click',()=> {endSession();offline('Servis selesai.');$('connectNote').textContent='Perangkat kembali RUN dan mute. Hubungkan kontrol untuk servis lagi.';});
window.addEventListener('blur',releasePad);
window.addEventListener('pagehide',endSession);
document.addEventListener('visibilitychange',()=> {if(document.hidden&&token){endSession();offline('Panel ditutup.');}});
setInterval(async ()=> {
  if(!token||beating||document.hidden)return;
  const session=token;beating=true;
  try {const data=await request('/api/action',{action:'heartbeat',heldPad},session);if(session===token)render(data.state);}
  catch(error){if(session===token)offline(error.message);}finally{beating=false;}
},400);
async function poll() {
  if(polling||document.hidden)return;polling=true;
  try {
    const data=await request('/api/state');
    if(token)render(data);
    else{$('connection').textContent='PERANGKAT TERSEDIA';$('connectNote').textContent=data.sessionActive?'Panel sedang digunakan. Sesi yang terputus akan berakhir otomatis.':'Siap. Hubungkan kontrol untuk masuk SERVICE dalam keadaan mute.';}
  }catch(error){if(token)offline('Perangkat tidak merespons.');else{$('connection').textContent='BELUM TERHUBUNG';$('connectNote').textContent='Pastikan HP memakai Wi-Fi ATMOS-CP-300-EXP dan tetap tersambung meski tanpa internet.';}}
  finally{polling=false;}
}
poll();setInterval(poll,1000);
