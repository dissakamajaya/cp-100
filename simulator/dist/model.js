export const STORAGE_KEY = 'chladni-08-presets-v4';
export const HARMONICS = ['odd', 'even', 'both'];
export const WAVES = ['sine', 'sawtooth', 'triangle'];
export const LIMITS = { frequencyHz: [20, 4000], levelPct: [0, 100], foldPct: [0, 100] };
export const SPEC = Object.freeze({ plateCm: 60, glassDiameterCm: 50, gapMm: 30, revision: '07' });
export const DEFAULT_PARAMETERS = Object.freeze({ frequencyHz: 128.5, levelPct: 35, wave: 'sine', foldPct: 0, harmonics: 'odd' });
export function validParameters(p) {
  return Boolean(p && typeof p === 'object' && WAVES.includes(p.wave) && HARMONICS.includes(p.harmonics)
    && Object.entries(LIMITS).every(([key, [min, max]]) => typeof p[key] === 'number' && Number.isFinite(p[key]) && p[key] >= min && p[key] <= max));
}
export function cleanParameters(p) {
  if (!validParameters(p)) throw new Error('Parameter preset tidak valid.');
  return { frequencyHz: Math.round(p.frequencyHz * 10) / 10, levelPct: Math.round(p.levelPct), wave: p.wave, foldPct: Math.round(p.foldPct * 10) / 10, harmonics: p.harmonics };
}
export function examplePresets() {
  return [
    { frequencyHz: 128.5, levelPct: 35, wave: 'sine', foldPct: 0, harmonics: 'odd' },
    { frequencyHz: 196, levelPct: 35, wave: 'triangle', foldPct: 18, harmonics: 'odd' },
    { frequencyHz: 256, levelPct: 30, wave: 'sawtooth', foldPct: 32, harmonics: 'both' },
    { frequencyHz: 384, levelPct: 40, wave: 'sine', foldPct: 58, harmonics: 'both' }, null, null, null, null,
  ];
}
export function parsePresetFile(value) {
  const object = typeof value === 'string' ? JSON.parse(value) : value;
  if (!object || object.schema !== 'chladni-08' || !Array.isArray(object.presets) || object.presets.length !== 8) {
    throw new Error('File preset harus memiliki tepat 8 slot CP-100.');
  }
  if (object.version === 3 && object.folding === 'triangle-fold-v1') {
    return object.presets.map(p => {
      if (p === null) return null;
      if (!['symmetric', 'asymmetric'].includes(p.foldMode)) throw new Error('Mode preset lama tidak valid.');
      return cleanParameters({...p, harmonics: p.foldMode === 'symmetric' ? 'odd' : 'both'});
    });
  }
  if (object.version !== 4 || object.folding !== 'parity-fold-v2') throw new Error('Gunakan preset CP-100 versi 4 atau versi 3.');
  return object.presets.map(p => p === null ? null : cleanParameters(p));
}
export function exportPresetFile(presets) {
  return { schema: 'chladni-08', version: 4, folding: 'parity-fold-v2', presets: presets.map(p => p ? cleanParameters(p) : null) };
}
export const CONFIG_KEY = 'chladni-08-setup-v1';
export const DEFAULT_SETUP = Object.freeze({minHz:20,maxHz:4000,calibrated:false,ledColor:'alternate',ledPulse:false});
export function validSetup(p){return Boolean(p&&Number.isFinite(p.minHz)&&Number.isFinite(p.maxHz)&&p.minHz>=20&&p.maxHz<=4000&&p.minHz<p.maxHz&&typeof p.calibrated==='boolean'&&['pink','tosca','alternate'].includes(p.ledColor)&&typeof p.ledPulse==='boolean');}
export function inRange(p,setup){return p.frequencyHz>=setup.minHz&&p.frequencyHz<=setup.maxHz;}
export function ledState(state,index){
 const active=state.activePad===index&&!state.muted;
 const color=active?(state.setup.ledColor==='alternate'?(index%2?'tosca':'pink'):state.setup.ledColor):'white';
 const fraction=(state.parameters.frequencyHz-state.setup.minHz)/(state.setup.maxHz-state.setup.minHz);
 return {active,color,pulseHz:active&&state.setup.ledPulse?.5+Math.max(0,Math.min(1,fraction))*1.5:0};
}
export function initialState(presets = examplePresets(), setup = DEFAULT_SETUP) {
  setup=validSetup(setup)?{...setup}:{...DEFAULT_SETUP};
  return { mode: 'RUN', muted: true, armed: false, activePad: null, setup, parameters: { ...DEFAULT_PARAMETERS,frequencyHz:Math.max(setup.minHz,Math.min(setup.maxHz,DEFAULT_PARAMETERS.frequencyHz)) }, presets: parsePresetFile(exportPresetFile(presets)) };
}
export function reduce(state, action) {
  switch (action.type) {
    case 'MODE':
      if (!['RUN', 'SERVICE'].includes(action.mode)) return state;
      return { ...state, mode: action.mode, muted: true, armed: false, activePad: null };
    case 'SETUP': {
      if(state.mode!=='SERVICE')return state;
      const setup={...state.setup,...action.patch};if(!validSetup(setup))return state;
      return {...state,setup,parameters:{...state.parameters,frequencyHz:Math.max(setup.minHz,Math.min(setup.maxHz,state.parameters.frequencyHz))},muted:true,armed:false,activePad:null};
    }
    case 'PARAMETERS': {
      if (state.mode !== 'SERVICE') return state;
      const p = { ...state.parameters, ...action.patch };
      if (!validParameters(p)) return state;
      p.frequencyHz=Math.max(state.setup.minHz,Math.min(state.setup.maxHz,p.frequencyHz));
      return { ...state, parameters: cleanParameters(p), activePad: null };
    }
    case 'ARM': return state.mode === 'SERVICE' ? { ...state, armed: !state.armed, muted:true,activePad:null } : state;
    case 'MUTE': {
      const muted=action.value===undefined?!state.muted:Boolean(action.value);
      if(!muted&&state.mode==='RUN')return state;
      return { ...state, muted, armed:false,activePad:null };
    }
    case 'SEQUENCE_STEP':
      // Move between pads in one update so audio automation never receives an intermediate mute.
      return reduce({...state,activePad:null,muted:true,armed:false},{type:'PAD_DOWN',index:action.index});
    case 'PAD_UP':
      return state.activePad===action.index?{...state,activePad:null,muted:true}:state;
    case 'RELEASE_ALL':return {...state,activePad:null,muted:true};
    case 'PAD':
    case 'PAD_DOWN': {
      const i = action.index;
      if (!Number.isInteger(i) || i < 0 || i > 7 || state.activePad!==null) return state;
      if (state.mode === 'SERVICE' && state.armed) {
        const presets = state.presets.map((p, index) => index === i ? { ...state.parameters } : p);
        return { ...state, presets, armed: false, activePad:null,muted:true };
      }
      if (!state.presets[i]||!inRange(state.presets[i],state.setup)) return state;
      return { ...state, parameters: { ...state.presets[i] }, activePad: i, muted: false };
    }
    case 'IMPORT':
      if (state.mode !== 'SERVICE') return state;
      try { return { ...state, presets: parsePresetFile(action.value), activePad: null, armed: false, muted: true }; } catch { return state; }
    default: return state;
  }
}
