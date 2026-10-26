# Mulai di sini

## Tujuan pekerjaan ini

Pengguna meminta satu repo GitHub dan folder lokal agar agent lain bisa melanjutkan progress CP-100. Paket ini mengonsolidasikan file yang relevan; pekerjaan desain dan publikasi yang tertunda tetap dicatat sebagai pekerjaan berikutnya.

## Kondisi terakhir yang ditemukan

- Source simulator diambil dari checkout Sites terbaru, bukan ZIP simulator lama.
- Simulator memiliki 8 pad momentary 4×2, audio DSP, sand preview, RUN/SERVICE, preset, RGB, dan batas sweep. 23 tes simulator dan 6 tes router lulus saat packaging.
- `outputs/CP-100-Rev08-Blueprint-dan-Wiring.pdf` tersedia (14 halaman), dengan template potong A3 1:1, SVG, DXF, CSV dan ZIP.
- PDF Rev08 yang ditemukan masih memakai `reference-render.png` pada cover dan judul “Referensi casing dari klien”. Kedua gambar masih asli. Ini belum memenuhi koreksi terakhir pengguna.
- Tidak ditemukan source firmware ESP32 (.ino/PlatformIO/ESP-IDF), Gerber, ataupun CAD produksi dalam bahan terpilih. Simulator bukan firmware perangkat.
- Bukti historis menyatakan Rev07 dan route domain telah lolos QA. Publikasi Rev08 belum terbukti selesai. File `simulator/dist/CP-100-Guide.pdf` sudah berisi Rev08 lokal; jangan menyamakan file itu dengan versi live.

## Instruksi pengguna terakhir yang belum selesai

1. Edit **dua** gambar casing menjadi panel hitam tanam dengan **8 pad 4×2**; kontrol servis tersembunyi.
2. Ganti judul “referensi casing dari klien” menjadi **“Build casing”**.
3. Gunakan cover awal yang disukai: `outputs/Chladni-08-Visual-Rev03.png`.
4. Rebuild PDF, periksa visual, regenerasi ZIP, lalu perbarui situs/PDF dan verifikasi versi yang benar-benar tayang.

Bahan edit: `work/rev8/reference-dimensions.png` dan `work/rev8/reference-render.png`. Keduanya sudah disalin dari folder sumber agar tidak bergantung pada lampiran sementara. Jangan memakai versi casing asli seolah revisi 8 pad sudah selesai.

## Keputusan final

Baca [docs/DECISIONS.md](docs/DECISIONS.md). Ukuran pelat terbaru **600×600 mm**, kaca **Ø500 mm**, celah awal **30 mm**. Angka 800×820×350 mm adalah acuan casing, bukan ukuran pelat. Beban kaca/sepatu diterima casing.

Diskusi ChatGPT tambahan masih memuat opsi MIDI keyboard 25-key/plate 100 cm dan catatan prototipe lama yang saling ambigu. Itu konteks eksplorasi; gunakan keputusan 8 pad/60 cm dari chat Codex sebagai rujukan revisi terakhir. Jangan memindahkan frekuensi prototipe lama menjadi batas aman perangkat final.

## Cara melanjutkan

1. Baca README, keputusan dan daftar pekerjaan berikut.
2. Jalankan tes. Gunakan Stagehand untuk semua pemeriksaan UI.
3. Kerjakan koreksi gambar dan PDF dahulu; jangan mengubah desain mekanik final tanpa dasar baru.
4. Pertahankan snapshot awal dalam Git. Builder menimpa PDF, pinout, blueprint dan ZIP.
5. Setelah perubahan PDF terverifikasi, salin PDF dan ZIP baru ke `simulator/dist/CP-100-Guide.pdf` dan `simulator/dist/CP-100-Blueprint-Build.zip`.
6. Untuk publikasi, buka skill Sites dan ambil checkout/credential resmi untuk project yang sama. Detail identitas non-rahasia ada di `docs/SOURCES.md`. Repo GitHub ini bukan source remote Sites; push GitHub tidak otomatis menerbitkan situs.
7. Catat commit, hasil tes, hash PDF, URL versi live, serta batas verifikasi pada handoff yang diperbarui.

## Suggested skills

- `imagegen`: wajib digunakan untuk edit gambar casing; view kedua gambar sebelum mengedit.
- `pdf`: render dan periksa halaman cover, Build casing, blueprint, wiring dan template sebelum merilis.
- `stagehand`: seluruh otomasi browser/QA melalui runner global.
- `sites:sites`: saat menyinkronkan dan menerbitkan source Sites yang sudah ada.
- `cloudflare` / `wrangler`: hanya bila perlu mengubah domain-router; route saat ini sudah dipaketkan beserta tes.

## Batas bukti

`evidence/historical/` merupakan salinan bukti terdahulu, bukan pengujian live tanggal 26 Oktober. Detail capture dan hash ada di `docs/SOURCE-MANIFEST.json`. Perubahan portability pada builder dan manifest foto dijelaskan di `docs/SOURCES.md`. Tidak ada credential yang diperlukan untuk menjalankan simulator lokal atau tes.

## Pembaruan audio web — 26 Oktober 2026

Permintaan pengguna: tingkat audio pada situs terlalu rendah. Attenuasi browser tetap `0.12` di `AudioPreview.update()` dihapus, sehingga amplitudo pada LEVEL yang sama naik 8,33× (+18,42 dB). Preset, firmware, normalisasi DSP, ramp dan mute tetap mengikuti perilaku sebelumnya.

Sites versi **7** berhasil dipublikasikan, commit source `cda11c0d66357d6ed61acd46215fcf53e7e184ef`, deployment `appgdep_6adf14cff1e08191bfbd960a801ad2fb`. Stagehand memverifikasi https://houseofexp.com/research/cp-100/ memakai hash DSP yang sama dengan source lokal, audio pad aktif (RMS 0,12616 pada LEVEL 35%), lepas pad = 0, serta 32 render Web Audio tanpa clipping (peak maksimum 0,73029). Tes simulator/router 29/29 dan manifest 80 aset lulus. Bukti, command dan batas verifikasi: [evidence/audio-boost-2026-10-26/README.md](evidence/audio-boost-2026-10-26/README.md).

Publikasi ini hanya mengubah audio web. Koreksi gambar/PDF Rev08 di atas tetap tertunda; tidak ada klaim verifikasi hardware.
