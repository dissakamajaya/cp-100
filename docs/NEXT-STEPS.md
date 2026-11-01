# Daftar kelanjutan

## Rilis minimal — 1 November 2026

- [x] Hapus display/LED eksternal dan kontrol LED dari firmware aktif; SERVICE seluruhnya di HP.
- [x] Buat PDF wiring empat pad + paket binary dengan checksum, offset dan flash helper.
- [x] Rev02: foto DevKit berpenanda pin, diagram kabel pad/PCM5102/daya; render dan periksa semua tujuh halaman.
- [ ] Uji hardware ESP32/empat pad/PCM5102/Wi-Fi; kalibrasi amplifier/shaker pada pelat nyata.
- [ ] Sinkronkan simulator publik dan dokumen Rev08 lama dengan revisi minimal; tidak ada deployment situs pada rilis repo ini.


## Firmware empat pad dan SERVICE HP — 31 Oktober 2026

- [x] Firmware empat pad, FREQ 20–4000 Hz dan FOLD 0,1%; migrasi empat slot pertama dan batas sweep lama.
- [x] Hotspot `ATMOS-CP-300-EXP`, UI offline di `192.168.4.1`; knob, selector, preset, setup sweep, sequencer dan GATHER.
- [x] Boot mute, timeout sesi/hold, pembatalan playback, laporan kegagalan penyimpanan, dan tes host portable.
- [ ] Upload pada ESP32-S3 nyata; uji Wi-Fi HP, audio I2S sambil mengubah kontrol cepat, disconnect/reconnect, restart/NVS, empat pad/MUTE fisik opsional.
- [ ] Kalibrasi batas sweep, output amplifier/shaker dan langkah GATHER pada pelat nyata.
- [ ] Sinkronkan simulator publik, blueprint/wiring, gambar dan PDF empat pad + SERVICE HP sebelum rilis dokumentasi/perangkat.

Petunjuk operator dan wiring terbaru: `firmware/README.md`. Bukti: `evidence/remote-service-2026-10-31/`.

## Revisi empat pad — 30 Oktober 2026

- [x] Temukan model 3D lokal di `enclosure/`; ubah panel pengunjung ke 4 pad 1×4 horizontal, 220×74×24 mm; ekspor STL dan simpan versi 8 pad.
- [ ] Cetak satu cap dan prototipe housing; uji clearance, return, travel, kabel dan sekrup dengan komponen aktual.
- [ ] Sinkronkan simulator publik, preset/sequencer/keyboard, blueprint/wiring dan dua gambar/PDF ke 4 pad sebelum rilis perangkat empat pad; firmware sudah diperbarui pada 31 Oktober. Instruksi 8 pad di daftar historis berikut perlu disesuaikan.

Lihat `enclosure/README.md` dan `evidence/enclosure-4-pad-row-2026-10-30/README.md`.

## Riwayat pekerjaan Rev08 (sesuaikan dengan revisi minimal di atas)

Butir panel fisik/MAX7219/MCP23017/LED di bawah sudah digantikan oleh SERVICE HP dan tidak perlu dirakit. PDF wiring minimal telah selesai; paket blueprint mekanik/simulator publik masih tertunda.

## 1. Selesaikan revisi visual/PDF

- [ ] Edit dua gambar casing yang disertakan menjadi 8 pad hitam tanam 4×2; pertahankan servis tersembunyi.
- [ ] Simpan gambar hasil edit sebagai file baru dan referensikan dalam builder PDF.
- [ ] Gunakan cover `outputs/Chladni-08-Visual-Rev03.png`.
- [ ] Ganti judul halaman 2 menjadi “Build casing”; sesuaikan catatan gambar asli yang sudah tidak benar setelah edit.
- [ ] Rebuild PDF/blueprint/pinout/ZIP.
- [ ] Render seluruh PDF; periksa cover, 8 pad, judul, angka dimensi, halaman servis, wiring, pagination dan template 1:1.

Selesai bila PDF dan ZIP konsisten, dua gambar sudah benar-benar diedit, cover disetujui sebelumnya digunakan, dan ada bukti visual baru.

## 2. Sinkronisasi situs

- [ ] Salin hasil terverifikasi ke `simulator/dist/`.
- [ ] Jalankan 29 tes dan QA browser desktop/mobile melalui Stagehand.
- [ ] Publikasikan ke Sites project yang sama memakai workflow resmi Sites.
- [ ] Verifikasi domain custom dan hash PDF yang diunduh dari versi live.
- [ ] Catat URL, commit, hash PDF, bukti QA, dan batas verifikasi.

## 3. Kesiapan perangkat

- [ ] Konfirmasi SKU dan ukur komponen aktual sebelum pemotongan.
- [ ] Buat satu prototipe pad untuk menguji return spring, stopper, travel, diffuser dan clearance.
- [ ] Pastikan kaca/rangka mendukung beban terpisah dari pelat.
- [ ] Verifikasi catu YS-502MT, shaker/speaker, grounding dan batas output aktual.
- [ ] Tentukan ketebalan/material pelat final serta batas frekuensi/level dari uji rakitan final.
- [x] Firmware ESP32 tahap bench (`firmware/`): penyimpanan preset, boot mute dan gate release.
- [x] Firmware: MCP23017 (MODE, WAVE, HARMONICS, FOLD), WS2812B, alur sweep tahan FREQ 2 dtk (diuji di papan simulasi dan emulator).
- [ ] Uji firmware pada ESP32-S3 + MAX7219 + PCM5102 mengikuti `firmware/README.md`; catat orientasi matrix, arah encoder, jumper PCM5102, dan hasil audio.
- [ ] Uji firmware dengan panel MCP23017 dan WS2812B nyata; setelah itu baru amplifier, shaker, dan pelat (mulai dari MUTE dan level rendah).
- [ ] Rancang PCB/carrier/Gerber dan CAD produksi setelah prototipe teruji.

Urutan perangkat di atas merupakan pekerjaan lanjutan; paket ini tidak menyatakan desain siap produksi.
