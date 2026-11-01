# Keputusan desain CP-100

| Bagian | Keputusan terakhir |
|---|---|
| Produk | CP-100 |
| Pelat | 600×600 mm, keputusan klien terakhir |
| Kaca | Bulat Ø500 mm; didukung rangka casing |
| Celah | Awal 30 mm; perlu verifikasi clearance fisik |
| Casing | Acuan gambar 800×820×350 mm; bukan ukuran produksi tervalidasi |
| Pengunjung | 4 pad tanam, 1×4 horizontal; tahan = bunyi, lepas = mute. Firmware remote sudah empat pad; simulator publik/PDF masih menunggu sinkronisasi. |
| Prioritas | Satu pad aktif, pad pertama yang ditahan memiliki output |
| Display / LED | Dihapus dari hardware aktif pada 1 November 2026; seluruh status di HP |
| Servis | Aplikasi web HP via hotspot ESP32 `ATMOS-CP-300-EXP`, alamat `http://192.168.4.1`; menggantikan panel fisik servis |
| Kontrol | 3 knob virtual + input angka: FREQ 20–4000 Hz / 0,1 Hz, LEVEL 0–100%, FOLD 0–100% / 0,1% |
| Selector | Di HP: WAVE SINE/SAW/TRIANGLE; HARMONICS ODD/EVEN/ODD+EVEN |
| Tombol | Di HP: SAVE per slot, TEST, MUTE/STOP, RUN/SERVICE; MUTE fisik GPIO16 opsional |
| Preset | Simpan frekuensi, level, wave, fold, harmonik ke pad tujuan |
| Sweep | Batas MIN/MAX tersimpan; nilai final menunggu uji pelat |

## Revisi firmware dan SERVICE HP — 31 Oktober 2026

Pengguna meminta firmware mengikuti spesifikasi terkini dan mengganti SERVICE menjadi remote lewat HP yang tersambung langsung ke hotspot ESP32. Nama hotspot dipilih pengguna: **ATMOS-CP-300-EXP**. Alamat lokal **192.168.4.1**, password acak per perangkat disimpan di NVS dan dibaca operator lewat serial `wifi`. UI lengkap tersimpan di ESP32; tanpa internet atau aplikasi terpasang.

Firmware kini empat pad, FREQ sampai 4000 Hz, FOLD 0,1%, sequence empat slot/3 detik urut atau acak, serta GATHER manual dengan editor langkah tersimpan. Ini menggantikan status historis “firmware tidak diubah” pada bagian bertanggal sebelumnya. GATHER tetap tidak otomatis saat idle. Batas sweep tersimpan dari firmware lama tidak diperluas otomatis; empat slot pertama dimigrasi tanpa menulis ulang saat boot.

Satu sesi kontrol aktif; timeout 2,5 detik kembali RUN/mute, hold remote timeout 1,2 detik, tidak ada auto-resume. Remote TEST/sequence/GATHER tidak merebut pad fisik yang sedang ditahan. Audio tetap di ESP32/PCM5102, bukan di HP. Encoder/selector/SAVE/MCP23017 panel lama tidak diinisialisasi. Driver lama disimpan sebagai riwayat; wiring terbaru ada di `firmware/README.md`.

Build, tes host dan UI lokal tidak membuktikan firmware sudah diuji di ESP32 nyata. PDF, diagram dan simulator publik masih perlu sinkronisasi terpisah.

## Revisi hardware minimal — 1 November 2026

Pengguna menegaskan panel servis fisik tidak diperlukan, sehingga display dan LED juga dihapus. Firmware aktif tidak menginisialisasi MAX7219/WS2812B/MCP23017/SPI/RMT; kontrol LED hilang dari UI/API. Buffer AHCT125 tidak dibutuhkan karena display/LED tidak digunakan. Rangkaian minimum: ESP32-S3 N16R8 + empat tombol NO + PCM5102 breakout + amplifier/shaker; MUTE GPIO16 opsional. Byte pengaturan LED lama tetap dibaca dalam migrasi NVS untuk kompatibilitas layout saja, tanpa output fisik.

Wiring saat ini: `output/pdf/CP-100-Wiring-4-Pad-WiFi.pdf` dan `firmware/README.md`. Bagian bertanggal 31 Oktober dan daftar komponen Rev08 di bawah adalah riwayat, bukan BOM revisi minimal.

## Acuan komponen

ESP32-S3 DevKitC-1 N16R8, PCM5102 I²S DAC, MAX7219 quad 32×8, YS-502MT amplifier, MCP23017, SN74AHCT125N, dan WS2812B. Untuk panel: Bourns PEC11R-4215F-S0024, C&K OS103011MA7QP1 / OS102011MA1QN1, Omron B3F-1000.

Komponen belum dibeli pada keputusan blueprint. Cocokkan datasheet, unit aktual, terminal fisik, tegangan dan polaritas amplifier sebelum fabrikasi/solder. Ukuran MAX7219 acuan 128×32×14 mm tidak membuktikan semua modul pasar identik.

## Mekanik blueprint

**Revisi terbaru, 30 Oktober 2026:** pengguna meminta model 3D controller menjadi 4 pad dari 8. Model di `enclosure/model.mjs` kini default 1×4 horizontal, casing 220×74×24 mm, face pad 40×40 mm, pitch 48 mm, PCB referensi custom 198×52 mm. STL lama 8 pad disimpan di `enclosure/stl/legacy-8-pad/`. Servis tetap terpisah/tersembunyi; pelat 600×600 mm dan kaca Ø500 mm tetap. Ukuran Rev08 di bawah adalah riwayat dan belum diperbarui dalam PDF. Belum ada uji cetak/hardware.

Panel pengunjung 220×122×2 mm; servis 260×180×2 mm. Pad cap 40×40 mm, jendela 41×41/R2; frame, cap, spring, PCB carrier dan stopper masih custom. Box pad target 24 mm dalam celah 30 mm menyisakan sekitar 6 mm sebelum memperhitungkan pasir dan gerak pelat.

Template potong adalah acuan prototipe. SVG/CSV memakai datum kiri atas (Y turun), DXF kiri bawah (Y naik). Cetak A3 pada 100% dan ukur kotak kalibrasi 50×50 mm. Ukuran panel/cutout usulan harus dibedakan dari dimensi komponen pada datasheet.

## Kontrol simulator web — 26 Oktober 2026

Atas permintaan pengguna, jangkauan simulator menjadi **20–4000 Hz** (default MAX 4000), FOLD bergerak per **0,1%**, dan frekuensi dapat diketik dengan presisi **0,1 Hz** melalui PANEL SERVIS. Batas sweep yang pernah disimpan tetap dipertahankan; MAX dapat diperluas lewat SIMPAN SETUP. Tombol keyboard angka 1–8 serta Numpad 1–8 (Num Lock aktif/nonaktif) menahan pad sampai tombol dilepas; input angka tidak memicu pad. Ini bukan perubahan batas aman hardware atau blueprint.

## Sequencer web — 26 Oktober 2026

Pengguna meminta tombol Play untuk sequence pad 1–8 setiap 3 detik dan opsi acak. Sequencer web berulang; acak mengunjungi setiap pad sekali per siklus. Pad kosong/di luar sweep diam selama slotnya. Stop, input pad manual, perubahan kontrol/servis, mute/audio-off dan meninggalkan halaman menghentikan sequence. Tidak ada auto-resume atau perubahan firmware.

## GATHER / kumpulkan pasir — 28 Oktober 2026

Masalah di pelat: pasir kadang lari ke samping. Pengguna meminta sequence untuk mengumpulkan pasir kembali ke tengah (tumpukan acak, bukan pola). Diputuskan: rutin terpisah dari 8 pad, hanya di PANEL SERVIS simulator, sine saja, sekali jalan dengan glide antar langkah lalu fade dan mute. Frekuensi harus dicari di pelat nyata (simulasi pasir hanya ilustratif). Periksa dulu kerataan pelat dan posisi klem di tengah speaker. Firmware menyusul setelah rutin terbukti di pelat (kandidat: jalan otomatis saat idle).
