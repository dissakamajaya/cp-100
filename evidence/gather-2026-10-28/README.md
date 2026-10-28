# GATHER publication — 28 October 2026 (Asia/Jakarta)

Published the existing GATHER implementation as a new version of the existing public CP-100 Sites project. The release contains `gather.js`, its integration in `app.js`, `model.js`, `index.html` and `styles.css`, plus the GATHER tests and simulator README. No application fixes were needed during this verification.

## Publication

- Project: `appgprj_6ad8a720c7bc81919ed7d22134c3c6aa` (public audience preserved).
- Sites source commit: `2f24f07020fecfc938db9d0b2373c5a8221ae181`.
- Version **10**: `appgprj_6ad8a720c7bc81919ed7d22134c3c6aa~appgver_2180d4346708819187c1da953c04eda4`.
- Deployment: `appgdep_6ae1b562ce308191bc56632e81b2c1c1`, native result **succeeded**.
- Deployed URL: https://chladni-08-lab.vocallime128.chatgpt.site
- Browser-verified live custom URL: https://houseofexp.com/research/cp-100/
- Native save/deploy results: `publication.json`. No credentials are stored in evidence.

## Verification

Before any publishing, the required simulator/router suite passed **44/44 tests** (`tests.txt`) and the source manifest passed **86 assets** (`manifest.txt`). JavaScript syntax and `git diff --check` also passed. The Sites helper reran the six GATHER tests in the source checkout before packaging.

Stagehand passed **28 checks locally** (`local.json`, `local-run.txt`) and **28 checks on the live custom URL** (`live.json`, `live-run.txt`), using the same saved workflow:

- PANEL SERVIS reveals KUMPULKAN PASIR / GATHER; ▶ GATHER enables audio and exposes ■ STOP.
- The full 40-second example routine plays all five steps in order, reaching each expected frequency and level with measured audio. Output stays sine, odd harmonics and zero fold.
- The final fade reduces level and measured audio, then playback stops and the model mutes. Final measured RMS/peak are zero. The unit tests independently verify the exact two-second fade and scheduler completion.
- STOP cancels playback and prevents a stale callback restarting it. A real pointer click on pad 1 stops GATHER and releases to silence; a held keyboard pad takes over, then mutes on release.
- TEST/MUTE and pad sequencer takeover stop GATHER. Presets remain unchanged.
- STOP during deliberately deferred native AudioContext activation prevents late playback.
- Mobile viewport 390×844 has no page overflow. Screenshots were visually inspected.
- SHA-256 hashes for all five published browser assets match the local source exactly.

Screenshots for each run: `*-playing.png`, `*-fade.png`, `*-complete.png`, and `*-mobile.png`. Structured results contain sampled playback phases, parameters and Web Audio analyser measurements. `local-failure.json` preserves an initial test-harness failure caused by using an unsupported locator method; the workflow was corrected to use Stagehand's supported pointer click, and the complete local run then passed.

## Commands

From `/Users/0xtx/Documents/GitHub/cp-100`:

```sh
node --test simulator/tests/*.test.js domain-router/worker.test.mjs
python3 scripts/verify_manifest.py
node --check simulator/dist/gather.js
node --check simulator/dist/app.js
node --check simulator/dist/model.js
git diff --check
python3 -m http.server 8080 --bind 127.0.0.1 --directory simulator/dist
stagehand-run /Users/0xtx/Documents/GitHub/cp-100/evidence/gather-2026-10-28/verify-gather.mjs
CP100_QA_URL=https://houseofexp.com/research/cp-100/ CP100_QA_LABEL=live stagehand-run /Users/0xtx/Documents/GitHub/cp-100/evidence/gather-2026-10-28/verify-gather.mjs
```

Sites source was opened and packaged with the official `site-workflow.mjs` helper in the ignored `scratch/sites-gather-checkout/`, with credentials passed only through hidden stdin. Git's initial fetch stalled; retrying with process-local `http.version=HTTP/1.1` completed. No global Git configuration was changed. Packaging used `commands: [["node", "--test", "tests/gather.test.js"]]` and archive `/Users/0xtx/Documents/GitHub/cp-100/scratch/gather-deployment.tar.gz`, then native `save_site_version` and `deploy_site_version` for the same project.

Browser checks ran in isolated sessions; no server-side visitor data was changed. This verifies simulator/browser behavior, not physical sand collection, hardware frequency limits, firmware or pending PDF/casing corrections. Unrelated firmware/enclosure work in the main checkout was preserved outside this release commit.
