# CP-100 web controls — 26 October 2026 (Asia/Jakarta)

Implemented the user's frequency range, finer FOLD, manual entry and keypad requests:

- Default/maximum supported web frequency: 20–4000 Hz. Saved narrower sweep bounds are retained and can be expanded through MAX / Hz → SIMPAN SETUP.
- FOLD: 0.1% per arrow/wheel increment and per 2 px drag (10× finer); Page Up/Down moves 1%. Fractional values survive preset storage/import/export.
- PANEL SERVIS has a numeric frequency field with 0.1 Hz precision, Enter or ATUR to apply, and native validation against active sweep bounds. RUN disables editing.
- Number-row and Numpad 1–8 hold the matching saved pad; release mutes. Numpad physical codes work with Num Lock off. Input fields retain typing, repeats/modifiers do not start a pad, and a second physical key cannot release the first key's pad.
- Existing amplified audio gain remains unchanged. These are web controls; no hardware limits, firmware, PDF or blueprint changes were made.

## Publication

- Sites project: `appgprj_6ad8a720c7bc81919ed7d22134c3c6aa`; existing public audience retained.
- Source commit: `e362c53ef57614bf3a0cc2c7473623911175b8a9`.
- Saved version: **8**, `appgprj_6ad8a720c7bc81919ed7d22134c3c6aa~appgver_4a156ff1ad748191b4ba1952272f2207`.
- Deployment: `appgdep_6adf269921088191a6d6c57077671c72`, native status `succeeded`.
- Upstream: https://chladni-08-lab.vocallime128.chatgpt.site
- Custom domain verified: https://houseofexp.com/research/cp-100/

## Verification

- Baseline: 29 tests / 80 manifest assets pass before edits.
- Updated: 32 simulator/router tests pass; 82 manifest assets match; JavaScript syntax and whitespace checks pass.
- Stagehand local and live: 29 checks pass in each run. `local.json` and `live.json` include all assertions, 24 keyboard combinations, preset state, Web Audio measurements and matching source hashes for app.js, model.js, controls.js, index.html and styles.css.
- Typed 1234.5 Hz via real keyboard input and Enter; typed 4000 via ATUR; 4000.1/19.9/empty rejected. FOLD arrow, wheel and pointer drag exercised through Stagehand.
- Saved/recall all eight presets at 4000 Hz and 1.2% FOLD, reload persistence, saved 2000 Hz ceiling preserved and re-expanded to 4000.
- Real OfflineAudioContext signal at 4000 Hz, 100% LEVEL, 0.1% FOLD: RMS 0.35993, peak 0.49944 (<1).
- Desktop/mobile screenshots: `local-desktop.png`, `local-mobile.png`, `live-desktop.png`, `live-mobile.png`. Live screenshots center the new frequency input; mobile width 390 px, no horizontal overflow.
- Initial local harness failed during a reload/toggle race. `local-failure.json` records it; the final workflow waits for load and opens the service panel only when hidden. No app repair was needed for this harness issue.

Commands from `/Users/0xtx/Documents/GitHub/cp-100`:

```sh
node --test simulator/tests/*.test.js domain-router/worker.test.mjs
python3 scripts/verify_manifest.py
node --check simulator/dist/app.js
git diff --check
stagehand-run /Users/0xtx/Documents/GitHub/cp-100/evidence/controls-2026-10-26/verify-controls.mjs
CP100_QA_URL=https://houseofexp.com/research/cp-100/ CP100_QA_LABEL=live stagehand-run /Users/0xtx/Documents/GitHub/cp-100/evidence/controls-2026-10-26/verify-controls.mjs
```

QA preset/setup writes affect only the isolated browser's localStorage. They do not change other visitors' presets.
