# Mulai di sini

## PDF wiring Rev02 - foto DevKit dan diagram kabel (1 November 2026)

Mengikuti koreksi pengguna, PDF terbaru kini **tujuh halaman**, termasuk foto DevKit N16R8 dengan lingkaran penanda pin/leader kabel (halaman 2), diagram header fisik ke empat pad/MUTE dan resistor pull-up (halaman 3), serta diagram DevKit ke PCM5102 berikut foto modul dan kedua terminal catu +5V/GND (halaman 4). J1/J3 dicocokkan dengan dokumentasi Espressif dan pin firmware. Foto referensi pemasok telah ada di bahan proyek; sumber/hak foto tetap dicantumkan, tidak berlisensi bebas.

File `output/pdf/CP-100-Wiring-4-Pad-WiFi.pdf` dan ZIP `firmware/releases/CP-100-remote-v1.0.0.zip` menggantikan versi empat halaman pada rilis awal. SHA256 PDF `89b94c83aaa212697ef493a585430d2093e7ae9c2facc89d272e24decbdbe860`; ZIP `13fde7f03c25055c227cbe73b48590325584f5add7491154f1c73dac69180b59`. Manifest ZIP sekarang mengikat hash PDF Rev02. **Seluruh binary/offset dan source firmware identik dengan v1.0.0**, tanpa perubahan firmware atau deployment situs.

Semua tujuh halaman dirender dan diperiksa; dua foto tertanam, nomor halaman dan batas gambar valid, ZIP CRC/checksum lulus. Baseline host sanitizer 326 cek, simulator/router 44 tes dan manifest 86 aset lulus sebelum edit. Bukti: [evidence/wiring-rev02-2026-11-01](evidence/wiring-rev02-2026-11-01/README.md). Rilis dokumentasi/binary terkait: [Wiring Rev02](https://github.com/dissakamajaya/cp-100/releases/tag/firmware-remote-v1.0.0-wiring-rev02). Belum ada uji hardware. Rilis awal dan hash PDF/ZIP di bagian berikut adalah riwayat.

## Rilis firmware minimal — 1 November 2026

Revisi terbaru mengikuti konfirmasi pengguna: **tidak ada panel servis fisik, display MAX7219, LED eksternal WS2812B, MCP23017, atau buffer AHCT125**. Hardware aktif: ESP32-S3 N16R8, empat pad NO GPIO1/2/4/5, PCM5102 BCK/LRCK/DIN GPIO18/21/47, amplifier/shaker. MUTE GPIO16 opsional. Status dan seluruh knob/adjustment tersedia di HP melalui hotspot **ATMOS-CP-300-EXP**, **http://192.168.4.1**. Driver lama dipertahankan sebagai riwayat dan dikecualikan dari build; byte LED lama hanya untuk kompatibilitas migrasi NVS.

Build terpin lulus tanpa warning: RAM **46.724 byte**, flash aplikasi **862.341 byte**. Tes DSP/state dan **326 cek kontrol/protokol/NVS** lulus dengan sanitizer; simulator/router **44/44**, manifest **86 aset**. Pemeriksaan UI terakhir membuktikan empat pad, pengaturan sweep tanpa LED, TEST/MUTE, layout HP 390×844 tanpa overflow dan keluar servis ke RUN/mute. Ini preview mesin C++ lokal, bukan pengujian Wi-Fi/audio fisik.

**Paket siap flash:** `firmware/releases/CP-100-remote-v1.0.0.zip`, empat image dengan offset dan SHA256, helper memvalidasi checksum sebelum upload. Region NVS tidak ditulis. Source/petunjuk: `firmware/README.md`. **PDF wiring terbaru:** `output/pdf/CP-100-Wiring-4-Pad-WiFi.pdf`, empat halaman telah dirender dan diperiksa. SHA256 PDF `13ffe855b3736387762167c856e33982961b7c4912e8f3b067f6fa14fd68eb29`; ZIP `23be12aed16b29d5e3558ae7461696a8b25811e0547bc4bf0ba4b51134c0d06c`.

Bukti terbaru: [evidence/release-2026-11-01/README.md](evidence/release-2026-11-01/README.md). Rilis GitHub telah tersedia: [firmware-remote-v1.0.0](https://github.com/dissakamajaya/cp-100/releases/tag/firmware-remote-v1.0.0) (prerelease, repo privat), source commit `74285937b2fbeeefb9bb8b75c385289c8ca334eb`, sudah di-push ke `main`. Ref main/tag dan SHA256 aset ZIP/PDF diverifikasi langsung dari GitHub. **Belum di-flash atau diuji pada ESP32 fisik.** Simulator publik dan blueprint Rev08 lama belum disinkronkan; rilis repo ini tidak menerbitkan Sites. Bagian bertanggal sebelumnya di bawah merupakan riwayat dan tidak menggantikan revisi minimal ini. Stagehand tidak lagi diwajibkan sesuai instruksi pengguna.

## Riwayat firmware empat pad + SERVICE HP — sebelum penghapusan display/LED

Permintaan pengguna 31 Oktober: perbarui firmware mengikuti spesifikasi terbaru dan pindahkan SERVICE ke HP melalui hotspot ESP32. SSID dipilih pengguna: **ATMOS-CP-300-EXP**, alamat **http://192.168.4.1**. Source di `firmware/` kini empat pad, FREQ 20–4000 Hz / 0,1 Hz, FOLD 0,1%, knob/selector virtual lengkap, preset/impor/ekspor, setup/LED, sequence empat slot/3 detik urut/acak, dan GATHER manual tersimpan. Aset UI offline di-embed dalam firmware. Encoder/selector/SAVE/MCP23017 lama tidak lagi dibaca; pad dan MUTE fisik GPIO16 tetap ada. Password dibuat per perangkat di NVS dan dibaca operator melalui serial `wifi`; tidak disimpan dalam source/API.

Migrasi NVS mempertahankan slot 1–4 dan batas sweep lama. Boot mute, satu sesi kontrol, timeout sesi 2,5 detik kembali RUN/mute, hold remote 1,2 detik; MUTE membatalkan playback dan antrean kontrol. Build terakhir tanpa warning: RAM **47.732 byte**, flash aplikasi **884.429 byte**. Tes DSP/state (9.009 entri kalibrasi, 72 kombinasi extended-range) dan **323 cek kontrol/protokol/NVS** lulus dengan ASan/UBSan; **13 cek UI** pada preview C++ lokal lulus; simulator/router **44/44**, manifest **86 aset** lulus. Tidak ada perangkat serial USB terdeteksi; **belum di-upload atau diuji di hardware**. Belum di-commit/push/deploy. Perubahan pengguna yang sudah ada dipertahankan.

Petunjuk operator, build/upload dan wiring terkini: [firmware/README.md](firmware/README.md). Command, hash, screenshot dan batas bukti: [evidence/remote-service-2026-10-31/README.md](evidence/remote-service-2026-10-31/README.md). Suite lama `firmware/test/sim/` tetap sebagai riwayat delapan pad; runner aktif portable ialah `sh firmware/test/run-host.sh --asan`. Status audit dan instruksi delapan pad pada bagian bertanggal sebelumnya adalah riwayat. Simulator publik/PDF/diagram belum disinkronkan; jangan memakai pinout Rev08 sebagai wiring empat pad + SERVICE HP.

## Pemeriksaan firmware — 31 Oktober 2026

Clean build ESP32-S3 dan tes host DSP/state lulus; build tanpa warning (RAM 23.448 byte, flash 367.725 byte). Tes simulator/router 44/44 dan manifest 86 aset lulus. Firmware masih delapan pad, batas 2000 Hz dan FOLD integer; sinkronisasi empat pad tetap tertunda. Sequencer/GATHER masih khusus simulator sesuai keputusan sebelumnya. Runner papan simulasi terhambat permission executable lalu opsi linker GNU `--wrap` yang tidak didukung macOS; ASan/UBSan belum berjalan. Ketiga referensi tes berbeda dari simulator terbaru, dan dokumentasi gain browser 0,12 sudah tertinggal. Tidak ditemukan port serial ESP32; tidak ada upload atau uji hardware. Source firmware tidak diubah. Detail, command, hash dan batas bukti: [laporan pemeriksaan](evidence/firmware-audit-2026-10-31/README.md) dan [results.json](evidence/firmware-audit-2026-10-31/results.json).

## Revisi terbaru — controller empat pad, 30 Oktober 2026

PNG untuk vendor: `enclosure/print-ready/CP-100-4-pad-vendor-preview.png` (2400×1800), render langsung STL enam part dengan dimensi layout dan assembly. Diverifikasi melalui Stagehand; bukti `evidence/enclosure-4-pad-row-2026-10-30/vendor-preview-results.json`.

Permintaan file STL siap cetak: `enclosure/print-ready/CP-100-4-pad-horizontal-PRINT.stl` berisi enam part terpisah (top, base, empat cap) di Z=0, footprint 220×208 mm. File housing dan empat cap terpisah juga tersedia untuk material/warna berbeda. Generator `enclosure/export-print-plate.mjs`; verifikasi binary STL `evidence/enclosure-4-pad-row-2026-10-30/verify-print-stl.py` dan hasil `print-stl-results.json`. Tetap perlu slicing sesuai printer dan uji fisik.

Pengguna meminta menemukan model 3D pad controller dan mengurangi 8 pad menjadi 4, lalu memilih satu baris horizontal (1×4), menggantikan asumsi awal 2×2. Model lokal ditemukan di `enclosure/` dan direvisi ke **1×4 horizontal**, dengan casing **220×74×24 mm**, face pad 40×40 mm dan pitch 48 mm. Source parametric `enclosure/model.mjs`, preview `enclosure/index.html`, STL serta paket `enclosure/CP-100-4-pad-STLs.zip` tersedia. Model/STL lama 8 pad dipertahankan; lihat `enclosure/README.md`. Servis terpisah tidak berubah. Keputusan empat pad ini menggantikan jumlah pad mekanik pada instruksi historis di bawah.

Firmware, simulator dan PDF belum disinkronkan ke 4 pad. Belum diuji cetak atau pada hardware. Bukti pemeriksaan geometri, preview Stagehand dan tes/manifest ada di `evidence/enclosure-4-pad-row-2026-10-30/README.md`.

## Tujuan pekerjaan ini

Pengguna meminta satu repo GitHub dan folder lokal agar agent lain bisa melanjutkan progress CP-100. Paket ini mengonsolidasikan file yang relevan; pekerjaan desain dan publikasi yang tertunda tetap dicatat sebagai pekerjaan berikutnya.

## Kondisi terakhir yang ditemukan

- Source simulator diambil dari checkout Sites terbaru, bukan ZIP simulator lama.
- Simulator memiliki 8 pad momentary 4×2, audio DSP, sand preview, RUN/SERVICE, preset, RGB, dan batas sweep. 23 tes simulator dan 6 tes router lulus saat packaging.
- `outputs/CP-100-Rev08-Blueprint-dan-Wiring.pdf` tersedia (14 halaman), dengan template potong A3 1:1, SVG, DXF, CSV dan ZIP.
- PDF Rev08 yang ditemukan masih memakai `reference-render.png` pada cover dan judul “Referensi casing dari klien”. Kedua gambar masih asli. Ini belum memenuhi koreksi terakhir pengguna.
- Tidak ditemukan Gerber ataupun CAD produksi dalam bahan terpilih. Simulator bukan firmware perangkat.
- `firmware/` (26 Oktober 2026): PlatformIO + Arduino untuk ESP32-S3 N16R8, pin Rev08. Semua kontrol Rev08 sudah ada: 8 pad, SAVE, MUTE/TEST, encoder FREQ/LEVEL, panel MCP23017 (MODE, WAVE, HARMONICS, FOLD), 8 WS2812B, editor sweep (tahan FREQ 2 dtk), display MAX7219, audio PCM5102, preset NVS, konsol serial. Verifikasi: tes paritas host dengan `dsp.js`/`model.js`; papan simulasi `firmware/test/sim/run.sh` (218 cek lulus, juga di bawah ASan/UBSan); build ESP32-S3 tanpa warning; boot dan sesi konsol lengkap di emulator QEMU esp32s3; review independen. **Belum diuji pada hardware nyata**; ikuti langkah uji di `firmware/README.md`.
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

## Kontrol web 4000 Hz, FOLD halus dan keyboard — 26 Oktober 2026

Sesuai permintaan pengguna, simulator web kini memiliki default jangkauan **20–4000 Hz**, FOLD per **0,1%** (drag, wheel dan panah), serta input frekuensi manual **0,1 Hz** di PANEL SERVIS (Enter / ATUR). Batas sweep yang sudah tersimpan tetap berlaku; ubah MAX ke 4000 lewat SIMPAN SETUP untuk memperluas batas lama. Preset menyimpan presisi FOLD baru. Keyboard angka 1–8 dan Numpad 1–8 (termasuk Num Lock nonaktif) menahan pad sampai tombol dilepas; input angka tetap dapat diketik tanpa memicu pad.

Sites versi **8** berhasil dipublikasikan, commit source `e362c53ef57614bf3a0cc2c7473623911175b8a9`, deployment `appgdep_6adf269921088191a6d6c57077671c72`. Domain custom terverifikasi melalui Stagehand: 29 pemeriksaan browser lulus, termasuk seluruh 24 kombinasi tombol, persistensi, batas lama, desktop/mobile dan hash source. Render Web Audio 4000 Hz mencapai RMS 0,35993 dan peak 0,49944 tanpa clipping. Tes simulator/router **32/32**, manifest **82** aset. Bukti dan command: [evidence/controls-2026-10-26/README.md](evidence/controls-2026-10-26/README.md). Firmware dan batas hardware tidak diubah.

## Sequencer web — 26 Oktober 2026

Ditambahkan Play/Stop di bawah pad, urutan 1→8 atau ACAK/RANDOM, berulang dengan **3 detik per slot**. Mode acak mengunjungi seluruh delapan pad per siklus; slot kosong/di luar batas tetap diam. Start menunggu clock audio aktif; transisi pad atomik. Stop, pad/keyboard manual, edit kontrol/servis, mute/audio-off dan meninggalkan halaman membatalkan timer, termasuk aktivasi audio yang masih pending. Tidak ada auto-resume.

Sites versi **9** berhasil dipublikasikan: commit source `690e1167b2553dc6216a02020ab96c06954c5c05`, deployment `appgdep_6adf2c03f460819195f7736f0f598f1a`. Stagehand pada domain custom https://houseofexp.com/research/cp-100/ meluluskan **31 pemeriksaan**, termasuk satu siklus berurutan kembali ke pad 1, satu siklus acak, timing nyata, audio dan pembatalan playback. **38 tes** simulator/router dan **84 aset** manifest lulus. Bukti dan command: [evidence/sequencer-2026-10-26/README.md](evidence/sequencer-2026-10-26/README.md). Firmware/PDF/batas hardware tidak diubah.

## GATHER / kumpulkan pasir — 28 Oktober 2026

Untuk masalah pasir lari ke samping, PANEL SERVIS simulator kini punya rutin **KUMPULKAN PASIR / GATHER**: daftar langkah sine (Hz, LEVEL, GLIDE detik, HOLD detik), main sekali, glide eksponensial antar langkah, fade 2 detik lalu mute. Editor: ubah angka langsung, **+ DARI FREQ** menambah langkah dari FREQ/LEVEL saat ini, **CONTOH** mengembalikan contoh (belum dikalibrasi). Kode: `simulator/dist/gather.js`, aksi `GATHER_FRAME` di `model.js`, UI di `app.js`/`index.html`/`styles.css`, tes `simulator/tests/gather.test.js`. Frekuensi final harus dicari di pelat nyata; firmware tidak diubah.

Sites versi **10** berhasil dipublikasikan pada project yang sama: commit source `2f24f07020fecfc938db9d0b2373c5a8221ae181`, deployment `appgdep_6ae1b562ce308191bc56632e81b2c1c1` (`succeeded`). **44 tes** simulator/router dan manifest **86 aset** lulus sebelum publikasi. Stagehand lokal dan live pada https://houseofexp.com/research/cp-100/ masing-masing meluluskan **28 pemeriksaan**, termasuk kelima langkah contoh 40 detik, fade akhir, audio menjadi nol, STOP, pad, pembatalan aktivasi audio, mobile dan hash lima aset. Bukti dan hasil verifikasi live dicatat di [evidence/gather-2026-10-28/README.md](evidence/gather-2026-10-28/README.md). Ini verifikasi simulator browser, bukan pengujian pengumpulan pasir di hardware.
