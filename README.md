# CP-100 / CP-300 — Chladni Plate Controller

Controller Chladni plate dari **EXP Research**. ESP32-S3 menghasilkan sinyal audio untuk menggetarkan pelat melalui DAC PCM5102, amplifier, dan transducer. Pengunjung memainkan empat pad horizontal. Operator mengatur suara dan preset dari HP melalui web SERVICE lokal.

| Varian | Mekanik | Status dokumentasi |
|---|---|---|
| CP-100 | Pelat 600 × 600 mm dan kaca bulat Ø500 mm | Acuan desain. Gambar casing pada blueprint Rev08 masih menunggu koreksi. |
| CP-300 | Casing silinder Ø500 × 262 mm, pelat aluminium 200 × 200 × 1 mm, jendela akrilik Ø200 mm dan controller di lid | Konfigurasi instalasi Atmos Jakarta dan Surabaya, EXP Research × 3HUNDRED. |

## Firmware v1.1.0

- Empat pad momentary dengan preset awal **1096 / 1360 / 2249 / 2386 Hz**. Satu pad aktif pada satu waktu. Melepas pad langsung memicu mute. Setiap penekanan dibatasi 5 detik, lalu pad harus dilepas sebelum dapat dimainkan kembali.
- SERVICE tersedia di [192.168.4.1](http://192.168.4.1) saat HP tersambung ke hotspot **ATMOS-CP-300-EXP**. Gunakan password operator dari pemilik unit. UI berjalan langsung di ESP32 dan mencakup penyetelan frekuensi, level, waveform, harmonik, fold, preset, TEST/MUTE, sequencer, dan GATHER.
- Seluruh tampilan dan pengaturan servis ada di HP. Hardware aktif memakai empat pad, ESP32-S3, DAC, amplifier, dan transducer. MUTE fisik bersifat opsional.

| Kebutuhan | Tautan |
|---|---|
| Build, flash, pinout, dan konfigurasi operator | [Panduan firmware](firmware/README.md) |
| Paket siap flash dan checksum | [GitHub prerelease v1.1.0](https://github.com/dissakamajaya/cp-100/releases/tag/firmware-remote-v1.1.0) |
| Diagram pad, DAC dan daya | [PDF wiring Rev04](output/pdf/CP-100-Wiring-4-Pad-WiFi.pdf) |
| Model 3D controller empat pad | [Enclosure](enclosure/README.md) |
| Blueprint dan PDF CP-100 Rev08 | [Arsip desain](outputs/) |
| Keputusan dan kelanjutan | [Keputusan desain](docs/DECISIONS.md) · [Daftar pekerjaan](docs/NEXT-STEPS.md) · [Handoff](HANDOFF.md) |

## Build dan verifikasi

Gunakan PlatformIO untuk ESP32-S3, Node.js 20+ untuk tes web, dan Python 3.10+ untuk manifest dan server simulator lokal.

```sh
sh firmware/test/run-host.sh --asan
pio run -d firmware
node --test simulator/tests/*.test.js domain-router/worker.test.mjs
python3 scripts/verify_manifest.py
```

Untuk mencoba simulator, jalankan `python3 -m http.server 8080 --bind 127.0.0.1 --directory simulator/dist`, lalu buka [simulator lokal](http://127.0.0.1:8080) dan aktifkan audio. Simulator memuat UI generasi sebelumnya. Hasil tes host dan simulasi mencakup perangkat lunak. Verifikasi GPIO, Wi-Fi, dan audio I²S firmware v1.1.0 pada ESP32 fisik masih tercatat sebagai pekerjaan lanjutan.
