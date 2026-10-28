import { writeFileSync, readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
const root = '/Users/0xtx/Documents/GitHub/cp-100';
const directory = `${root}/evidence/gather-2026-10-28`;
export default async ({ page, saveScreenshot }) => {
  const url = process.env.CP100_QA_URL || 'http://127.0.0.1:8080/';
  const label = process.env.CP100_QA_LABEL || 'local';
  const checks = [], samples = [];
  const check = (name, passed) => { checks.push({ name, passed: !!passed }); if (!passed) throw new Error(name); };
  const wait = ms => page.evaluate(ms => new Promise(resolve => setTimeout(resolve, ms)), ms);
  const diag = () => page.evaluate(() => window.chladniDiagnostics());
  const play = async () => {
    await page.locator('#gather-play').click();
    await page.evaluate(async () => { const end = performance.now() + 5000; while (window.chladniDiagnostics().gather.pending && performance.now() < end) await new Promise(r => setTimeout(r, 20)); });
    await wait(200);
  };
  const sample = duration => page.evaluate(async duration => {
    const output = [], start = performance.now();
    do {
      const d = window.chladniDiagnostics();
      output.push({ at: performance.now() - start, gather: d.gather, parameters: d.state.parameters, muted: d.state.muted, audio: d.audio });
      await new Promise(r => setTimeout(r, 100));
    } while (performance.now() - start < duration);
    return output;
  }, duration);
  try {
    await page.goto(url); await page.waitForLoadState('load');
    await page.evaluate(async () => { const end = Date.now() + 15000; while (typeof window.chladniDiagnostics !== 'function' && Date.now() < end) await new Promise(r => setTimeout(r, 50)); });
    await page.snapshot(); await page.setViewportSize(1280, 1000);
    check('Boot muted with GATHER stopped', (await diag()).state.muted && !(await diag()).gather.running);
    check('GATHER starts inside hidden service panel', await page.evaluate(() => document.querySelector('#service-panel').hidden));
    await page.locator('#maintenance-toggle').click(); await page.snapshot();
    check('PANEL SERVIS reveals KUMPULKAN PASIR and GATHER', await page.evaluate(() => !document.querySelector('#service-panel').hidden && document.querySelector('#gather-title').textContent.includes('KUMPULKAN PASIR') && document.querySelector('#gather-play').textContent === '▶ GATHER'));
    const presets = JSON.stringify((await diag()).state.presets);
    await play();
    check('GATHER starts in SERVICE with audio enabled', (await diag()).gather.running && (await diag()).state.mode === 'SERVICE' && (await diag()).audio.enabled);
    check('Running button exposes STOP', await page.evaluate(() => document.querySelector('#gather-play').textContent === '■ STOP'));
    await page.evaluate(() => document.querySelector('.gather').scrollIntoView({block: 'center'}));
    await saveScreenshot(`${directory}/${label}-playing.png`);
    samples.push(...await sample(38000));
    await saveScreenshot(`${directory}/${label}-fade.png`);
    samples.push(...await sample(2500));
    const holds = samples.filter(s => s.gather.frame?.phase === 'hold');
    check('All five example steps play in order', JSON.stringify([...new Set(holds.map(s => s.gather.frame.index))]) === '[0,1,2,3,4]');
    check('Every hold matches its frequency and level with audible output', [55.6,88.2,140,222.2,352.8].every((hz,i) => holds.some(s => s.gather.frame.index === i && s.parameters.frequencyHz === hz && s.parameters.levelPct === [40,36,32,26,20][i] && !s.muted && s.audio.rms > .03)));
    check('Playback stays plain sine without fold', samples.filter(s => s.gather.running).every(s => s.parameters.wave === 'sine' && s.parameters.foldPct === 0 && s.parameters.harmonics === 'odd'));
    const fades = samples.filter(s => s.gather.frame?.phase === 'fade');
    check('Final fade spans two seconds and reduces level and measured audio', fades.length >= 15 && fades[0].parameters.levelPct > 15 && fades.at(-1).parameters.levelPct <= 2 && fades[0].audio.rms > fades.at(-1).audio.rms * 3);
    let d = await diag();
    check('Completion stops scheduling and mutes actual audio', !d.gather.running && !d.gather.pending && d.state.muted && d.audio.rms < .00001);
    check('Completion reports GATHER SELESAI / MUTE', await page.evaluate(() => document.querySelector('#message').textContent.includes('GATHER SELESAI')));
    await saveScreenshot(`${directory}/${label}-complete.png`);
    await play(); await wait(600); await page.locator('#gather-play').click(); await wait(200); d = await diag();
    check('STOP halts GATHER and silences audio', !d.gather.running && !d.gather.pending && d.state.muted && d.audio.rms < .00001);
    await wait(2200); check('STOP cannot restart from a stale frame', !(await diag()).gather.running && (await diag()).state.muted);
    await play(); await page.locator('[data-pad="0"]').click(); await wait(200); d = await diag();
    check('Pad press stops GATHER', !d.gather.running && !d.gather.pending);
    check('Pad release leaves output muted', d.state.muted && d.audio.rms < .00001);
    await play(); await page.evaluate(() => document.body.dispatchEvent(new KeyboardEvent('keydown',{key:'1',code:'Digit1',bubbles:true}))); d = await diag();
    check('Held keyboard pad takes over GATHER', !d.gather.running && d.state.activePad === 0 && !d.state.muted);
    await page.evaluate(() => document.body.dispatchEvent(new KeyboardEvent('keyup',{key:'1',code:'Digit1',bubbles:true}))); await wait(200);
    check('Keyboard release mutes', (await diag()).state.muted && (await diag()).audio.rms < .00001);
    await play(); await page.locator('#mute-action').click(); check('TEST / MUTE cancels GATHER', !(await diag()).gather.running && (await diag()).state.muted);
    await play(); await page.locator('#sequence-play').click(); check('Pad sequence replaces GATHER', !(await diag()).gather.running && (await diag()).sequence.running);
    await page.locator('#sequence-play').click();
    check('Presets unchanged throughout GATHER and interruptions', JSON.stringify((await diag()).state.presets) === presets);
    await page.setViewportSize(390,844); await page.evaluate(() => document.querySelector('.gather').scrollIntoView({block:'center'}));
    check('Mobile has no page overflow', await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
    await saveScreenshot(`${directory}/${label}-mobile.png`);
    // Defer the native audio resume to verify STOP also cancels activation.
    await page.goto(url); await page.waitForLoadState('load'); await page.snapshot();
    await page.locator('#maintenance-toggle').click();
    await page.evaluate(() => { window.originalResume = AudioContext.prototype.resume; AudioContext.prototype.resume = function () { const context = this; return new Promise((resolve,reject) => { window.finishResume = () => window.originalResume.call(context).then(resolve,reject); }); }; });
    await page.locator('#gather-play').click(); check('Pending activation exposes STOP', (await diag()).gather.pending);
    await page.locator('#gather-play').click(); await page.evaluate(() => { AudioContext.prototype.resume = window.originalResume; window.finishResume(); }); await wait(1000);
    check('Late activation cannot restart GATHER after STOP', !(await diag()).gather.running && !(await diag()).gather.pending && (await diag()).state.muted);
    const hashes = {};
    for (const name of ['gather.js','app.js','model.js','index.html','styles.css']) {
      const content = await page.evaluate(async name => await (await fetch(`./${name}`,{cache:'no-store'})).text(), name);
      hashes[name] = createHash('sha256').update(content).digest('hex');
      check(`Source hash matches: ${name}`, hashes[name] === createHash('sha256').update(readFileSync(`${root}/simulator/dist/${name}`)).digest('hex'));
    }
    writeFileSync(`${directory}/${label}.json`,JSON.stringify({url,label,passed:true,checks,hashes,samples},null,2));
    return {url,label,passed:true,checks:checks.length};
  } catch (error) {
    writeFileSync(`${directory}/${label}-failure.json`,JSON.stringify({url,label,checks,samples,diagnostics:await diag().catch(()=>null),error:String(error)},null,2)); throw error;
  }
};
