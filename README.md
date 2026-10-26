# CP-100 — agent handoff

Paket kelanjutan controller Chladni House of EXP. Snapshot 26 Oktober 2026 (Asia/Jakarta), dikumpulkan dari chat sumber dan file lokal. Mulai dari [HANDOFF.md](HANDOFF.md), lalu [NEXT-STEPS.md](docs/NEXT-STEPS.md).

**Status:** simulator dan blueprint Rev08 tersedia. Gambar casing di PDF Rev08 masih belum mengikuti revisi terakhir pengguna. Firmware ESP32, PCB produksi, dan uji perangkat fisik belum tersedia dalam paket ini. Publikasi Rev08 belum terverifikasi pada handoff ini.

| Lokasi | Isi |
|---|---|
| `simulator/` | Source simulator terbaru dari checkout Sites, `dist/` dan 23 tes |
| `domain-router/` | Worker route custom domain dan 6 tes |
| `outputs/` | PDF Rev07/Rev08, pinout, blueprint SVG/DXF/CSV/A3, ZIP, cover pilihan |
| `work/` | Builder PDF Rev08, bahan gambar, foto komponen dan sumber dimensinya |
| `docs/` | Keputusan, daftar kerja berikut, sumber, dan manifest file |
| `evidence/` | Hasil verifikasi handoff dan bukti QA historis |

## Jalankan

Node.js 20+ dan Python 3.10+ dianjurkan. Simulator tidak memerlukan paket npm.

```sh
node --test simulator/tests/*.test.js domain-router/worker.test.mjs
python3 -m http.server 8080 --bind 127.0.0.1 --directory simulator/dist
```

Buka `http://127.0.0.1:8080`. Browser meminta interaksi pengguna untuk mengaktifkan audio. Semua otomasi browser harus melalui Stagehand; lihat [AGENTS.md](AGENTS.md).

## PDF

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-pdf.txt
.venv/bin/python work/build_rev8.py
```

Builder menimpa file Rev08 di `outputs/`; commit/salin snapshot sebelum rebuild. Versi ini masih menggunakan Arial, Arial Bold, dan Andale Mono dari `/System/Library/Fonts/Supplemental/` pada macOS. Untuk OS lain, sediakan font kompatibel dan sesuaikan pemetaan font pada `work/build_rev5_concise.py`. Jangan memasukkan font berlisensi ke repo tanpa izin. Builder memakai tiga halaman wiring dari PDF Rev07 yang disertakan.

Rebuild saja belum menyelesaikan koreksi gambar/cover. Lanjutkan sesuai [HANDOFF.md](HANDOFF.md).

## Tautan

- Simulator historis: https://houseofexp.com/research/cp-100/
- PDF historis: https://houseofexp.com/research/cp-100/CP-100-Guide.pdf
- Chat Codex: `codex://threads/01a15d32-d71c-77e1-bce0-aead509b9348`
- Diskusi awal: https://chatgpt.com/share/6adedb0c-ff7c-83ec-8d88-d171cdc78917

Tautan publik dicatat sebagai referensi; status live belum diuji ulang dalam pekerjaan packaging ini. Foto pihak ketiga tetap memiliki hak pemilik masing-masing; sumber tercantum dalam manifest foto dan PDF.
