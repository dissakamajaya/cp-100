# CP-100 web sequencer — 26 October 2026 (Asia/Jakarta)

The user requested a Play button to sequence pads 1–8 every three seconds, with a random option.

## Behavior

- PLAY enables browser audio, waits for the audio clock to advance, then starts the first slot immediately. STOP remains available during activation.
- Ordered mode loops 1→2→…→8→1. Random mode shuffles all eight slots per cycle and avoids an immediate repeated pad at the cycle boundary.
- Each slot lasts 3000 ms. Empty or out-of-range slots are silent for their slot, and the outline/status still identifies the sequencer position.
- A single model transition recalls each new pad, so parameter changes do not issue an intermediate audio mute. SAVE is disarmed without writing a preset.
- STOP, manual pads/keyboard, changes to order/mode/setup/parameters, SAVE/import, TEST/MUTE, audio-off, page blur/hide and navigation halt the timer. A pending audio activation cannot start playback after cancellation. Reload never resumes playback.
- Preset data, frequency/FOLD controls, audio gain, firmware, PDF and hardware limits are unchanged.

## Publication

- Existing public Sites project: `appgprj_6ad8a720c7bc81919ed7d22134c3c6aa`.
- Source commit: `690e1167b2553dc6216a02020ab96c06954c5c05`.
- Saved version: **9**, `appgprj_6ad8a720c7bc81919ed7d22134c3c6aa~appgver_e3e8a63c09a481919b3695218f89097e`.
- Deployment: `appgdep_6adf2c03f460819195f7736f0f598f1a`, native result `succeeded`.
- Upstream: https://chladni-08-lab.vocallime128.chatgpt.site
- Custom URL: https://houseofexp.com/research/cp-100/

## Verification

- Baseline: 32 tests / 82 source assets passed before editing.
- Updated: **38 simulator/router tests** pass (`tests.txt`), including exact fake-clock timing, all-eight looping, random cycles, stale callbacks, empty/out-of-range silence, atomic transitions and SAVE protection.
- `python3 scripts/verify_manifest.py`: **84 assets** match. Syntax and whitespace checks pass.
- Stagehand local browser: **31 checks pass** (`local.json`). Records nine ordered steps (including wraparound), eight random steps, real-time timing, actual audio at startup, silence after Stop, all stop/override controls, mobile overflow, a deliberately deferred audio resume cancelled by Stop, and exact source hashes for six changed browser assets.
- Live custom-domain browser verification: **31 checks pass** (`live.json`), with the same ordered/random cycles, timing, audio, stop controls and exact source hashes.
- Screenshots: `local-desktop.png`, `local-mobile.png`; `live-desktop.png`, `live-mobile.png`.
- Browser QA caught and fixed native timer receiver binding, output-device startup latency, and TEST/MUTE toggling back on after stop. The last pre-fix failure is retained in `local-failure.json`; the final local result passes.

Commands from `/Users/0xtx/Documents/GitHub/cp-100`:

```sh
node --test simulator/tests/*.test.js domain-router/worker.test.mjs
python3 scripts/verify_manifest.py
node --check simulator/dist/app.js
git diff --check
stagehand-run /Users/0xtx/Documents/GitHub/cp-100/evidence/sequencer-2026-10-26/verify-sequencer.mjs
CP100_QA_URL=https://houseofexp.com/research/cp-100/ CP100_QA_LABEL=live stagehand-run /Users/0xtx/Documents/GitHub/cp-100/evidence/sequencer-2026-10-26/verify-sequencer.mjs
```

Browser checks operate in an isolated session. No visitor presets or live server data are written. This is browser/simulator verification, not hardware verification.
