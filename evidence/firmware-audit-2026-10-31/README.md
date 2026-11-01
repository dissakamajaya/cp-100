# Pemeriksaan firmware ESP32 — 31 Oktober 2026

Firmware lokal **berhasil di-build untuk ESP32-S3 dan lulus tes host**, tetapi belum disinkronkan dengan controller empat pad terbaru. Tidak ada upload atau pengujian board fisik pada pemeriksaan ini. Source firmware tidak diubah.

## Snapshot

- Checkout `main`, HEAD `0f4a0d5`, satu commit di depan `origin/main`.
- `firmware/` masih untracked. Perubahan README, HANDOFF, keputusan, daftar pekerjaan, `.gitignore`, enclosure, dan bukti enclosure sudah ada sebelum pemeriksaan; dipertahankan.
- Target PlatformIO: `espressif32@6.9.0`, Arduino 2.0.17, ESP32-S3, konfigurasi flash QIO 16 MB / PSRAM OPI.
- Hash source, binary, perbandingan referensi, serta ringkasan hasil ada di [results.json](results.json).

## Temuan

1. **Belum sesuai casing empat pad.** `firmware/src/model.h:33`, `include/pins.h:12`, dan `src/inputs.cpp:31` masih menggunakan delapan pad; preset, input, LED, konsol dan NVS masih mengikuti rancangan Rev08. Mengubah `kPadCount` saja belum cukup karena array/input masih hardcoded delapan. Sinkronisasi empat pad sudah tercatat sebagai pekerjaan tertunda di keputusan proyek.
2. **Paritas hanya untuk fitur lama.** `src/model.h:16` menyimpan FOLD sebagai integer, `model.h:34` membatasi frekuensi sampai 2000 Hz; konsol `main.cpp:427` menolak FOLD pecahan. Simulator sekarang mendukung 4000 Hz dan FOLD 0,1%. Sequencer dan GATHER juga belum ada di firmware. Keputusan proyek memang membatasi pembaruan tersebut pada simulator; ini keterbatasan yang diketahui, bukan alasan menaikkan batas hardware tanpa kalibrasi.
3. **Papan simulasi belum bisa diverifikasi ulang dengan runner yang tersedia di macOS.** Perintah README `./run.sh` ditolak karena file tidak executable. Menjalankan melalui `sh` mencapai linker, lalu gagal pada `-Wl,--wrap=_Z14calibrationFor4Wavei9Harmonics` di `test/sim/sim.mk:12`, yang tidak didukung Apple linker. ASan/UBSan dan skenario papan tidak berjalan. Angka historis 218 cek bukan hasil pemeriksaan ini.
4. **Referensi tes tertinggal.** Ketiga salinan `test/sim/ref/app.js`, `dsp.mjs`, `model.mjs` berbeda dari `simulator/dist/` saat ini. Runner mengecek kesamaan byte ketiganya, sehingga tetap akan melaporkan mismatch ketika masalah linker selesai. Tes host yang lulus memakai fixture yang disertakan; bukan bukti seluruh fitur simulator terbaru sudah sama.
5. **Dokumentasi gain audio tertinggal.** `firmware/README.md:106` dan komentar `src/audio.h:11` masih mengacu pada gain browser 0,12 dan perbandingan +12 dB. Simulator terbaru telah menghapus attenuasi tersebut. Firmware masih memakai trim 0,5, sehingga faktor amplitudo digital trim-nya adalah sekitar −6,02 dB terhadap simulator pada parameter yang sama. Ini bukan pengukuran tegangan DAC atau volume fisik.

Perbandingan tambahan terhadap tabel kalibrasi yang dibangkitkan dari DSP terbaru mencakup 909 entri. Selisih mean maksimum hanya sekitar presisi numerik, dan gain identik pada presisi teks yang dihasilkan; nilai tepat ada di `results.json`. Perbedaan file referensi tidak dengan sendirinya membuktikan algoritma fold/calibration rusak.

## Verifikasi dan perintah

Semua perintah dijalankan dari `/Users/0xtx/Documents/GitHub/cp-100`.

```sh
pio run -d firmware -t clean
pio run -d firmware
g++ -std=c++17 -Wall -Wextra -Ifirmware/src firmware/test/host_test.cpp firmware/src/dsp.cpp firmware/src/model.cpp -o /tmp/cp100-audit-host-test
/tmp/cp100-audit-host-test
sh firmware/test/sim/run.sh --asan
node --test simulator/tests/*.test.js domain-router/worker.test.mjs
python3 scripts/verify_manifest.py
pio device list
node firmware/tools/gen_calibration.mjs simulator/dist/dsp.js > /tmp/cp100-audit-calibration.h
```

| Pemeriksaan | Hasil baru |
|---|---|
| Clean build PlatformIO | PASS, tanpa warning; RAM 23.448 byte (7,2%), flash 367.725 byte (5,6% partisi aplikasi) |
| Host DSP/state | PASS, `all checks passed`; termasuk boot mute, release, prioritas pad, SAVE dan sweep |
| Papan simulasi / ASan / UBSan | BLOCKED pada linker macOS; bukan PASS |
| Simulator dan router | 44 PASS, 0 FAIL |
| Manifest | 86 aset PASS; bukan pemeriksaan kelengkapan firmware |
| Port board ESP32 | Tidak terdeteksi pada daftar port serial |

Log mentah lokal: `platformio-clean.log`, `platformio-build.log`, `host-test.log`, `board-sim.log`, `simulator-router-tests.log`, `manifest.log`, dan `serial-devices.log` pada direktori ini. Ekstensi `.log` diabaikan oleh Git global mesin; ringkasan JSON/Markdown disimpan terpisah agar bukti tetap dapat ditinjau.

Tidak ada verifikasi ulang QEMU, browser, output audio fisik, panel, amplifier, shaker, atau pelat. Pengujian host dan build tidak menggantikan pengujian hardware.
