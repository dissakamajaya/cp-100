# Instruksi agent CP-100

Mulai dengan README.md, HANDOFF.md, docs/DECISIONS.md dan docs/NEXT-STEPS.md. Pertahankan keputusan terbaru: 8 pad, pelat 600×600 mm, kaca Ø500 mm, servis tersembunyi. Jangan menganggap PDF Rev08 awal sudah memenuhi koreksi gambar terakhir.

Sebelum mengedit, periksa branch/status Git dan hasil tes. Jangan revert perubahan pengguna/agent lain. Update HANDOFF dan bukti verifikasi setelah perubahan yang relevan. Jangan menyimpan credential atau menerbitkan foto/referensi sebagai aset bebas lisensi.

## Global browser automation policy

For every browser, website, web UI, navigation, interaction, extraction, screenshot, or browser-QA task, use the global Stagehand runner:

```sh
stagehand-run /absolute/path/to/workflow.mjs
```

Never use Playwright, playwright-cli, generic browser tools, in-app browser tools, Chrome plugins, direct Chrome control, agent-browser, Puppeteer, or OS-level GUI automation for browser pages. If Stagehand cannot perform the task, report the blocker instead of falling back.

Load `~/.codex/skills/stagehand/SKILL.md` for workflow conventions. This policy applies to all agents, reviewers and QA workers. Use explicit absolute workflow paths; save evidence files and record the command. Native service connectors/CLI may be used for non-browser GitHub/Sites/Cloudflare operations.

## Verification

```sh
node --test simulator/tests/*.test.js domain-router/worker.test.mjs
python3 scripts/verify_manifest.py
```

Tes simulator bukan bukti hardware. QA historis bukan bukti live terbaru. Push GitHub bukan deployment Sites. Selesaikan koreksi visual, render PDF dan periksa output sebelum publikasi.
