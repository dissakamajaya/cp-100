# CP-100 browser audio boost — 26 October 2026 (Asia/Jakarta)

Removed the browser-only 0.12 gain multiplier in `simulator/dist/dsp.js` and the existing Sites source. At the same LEVEL setting, steady output amplitude increases by 1/0.12 = 8.3333 (+18.416 dB). Calibration, saved presets, synthesis, muted boot, output ramping and pad-release behavior are retained. This is browser audio verification, not hardware verification.

## Publication

- Project: `appgprj_6ad8a720c7bc81919ed7d22134c3c6aa`, existing public audience retained.
- Sites source commit: `cda11c0d66357d6ed61acd46215fcf53e7e184ef`.
- Saved version: **7** (`appgprj_6ad8a720c7bc81919ed7d22134c3c6aa~appgver_e22bd1c905d08191bdf34bd35335c65e`).
- Deployment: `appgdep_6adf14cff1e08191bfbd960a801ad2fb`, native result `succeeded`.
- Upstream: https://chladni-08-lab.vocallime128.chatgpt.site
- Custom URL verified: https://houseofexp.com/research/cp-100/
- Live/local DSP SHA-256: `1f0b96b27f98ba88b7a724fe984c2a571604344ee5ba90178afff3856d3873f3`.

Only `dist/dsp.js` changed in the Sites source. Existing PDF/ZIP assets were retained; this publication does not complete the pending Rev08 visual corrections.

## Verification

- Before edits: 29 simulator/router tests passed; all 80 manifest assets matched.
- After edits: 29 simulator/router tests passed; the 23 hosted-source simulator tests also passed during packaging.
- `python3 scripts/verify_manifest.py`: PASS, 80 assets. The DSP entry records the new packaged hash and revision note while retaining its original source hash.
- Stagehand browser QA: `local.json` / `local.png` and `live.json` / `live.png`.
- Real-time live pad 1: RMS 0.126159 at LEVEL 35%; release RMS 0. Boot muted; audio-off disabled.
- 32 OfflineAudioContext cases rendered through the actual AudioPreview graph: SINE/SAW/TRIANGLE, ODD/EVEN/BOTH, FOLD 0/40/100, frequency endpoints, LEVEL 0/35/50/100. Maximum tested peak 0.730285 (<1), zero level silent; clean 256 Hz sine RMS follows 0.36 × LEVEL/100.
- Sawtooth/folded browser RMS varies from ideal periodic calibration because Web Audio oscillators are band-limited; QA permits that existing variation.
- Additional 909 ideal waveform cases (all integer fold settings, three waves, three harmonic selections) at LEVEL 100: maximum peak 0.7772003.

Commands from `/Users/0xtx/Documents/GitHub/cp-100`:

```sh
node --test simulator/tests/*.test.js domain-router/worker.test.mjs
python3 scripts/verify_manifest.py
stagehand-run /Users/0xtx/Documents/GitHub/cp-100/evidence/audio-boost-2026-10-26/verify-audio.mjs
CP100_QA_URL=https://houseofexp.com/research/cp-100/ CP100_QA_LABEL=live stagehand-run /Users/0xtx/Documents/GitHub/cp-100/evidence/audio-boost-2026-10-26/verify-audio.mjs
```

The first local QA attempt failed due to the test dispatching keyboard events on Document instead of the normal body target, plus an overly narrow RMS tolerance for band-limited sawtooth. The first live attempt ran before the app module finished loading. The retained workflow fixes those harness issues, waits for app readiness, and the final local/live results pass. No source changes were required for those harness corrections.
