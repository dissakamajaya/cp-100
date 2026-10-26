# Keputusan desain CP-100

| Bagian | Keputusan terakhir |
|---|---|
| Produk | CP-100 |
| Pelat | 600×600 mm, keputusan klien terakhir |
| Kaca | Bulat Ø500 mm; didukung rangka casing |
| Celah | Awal 30 mm; perlu verifikasi clearance fisik |
| Casing | Acuan gambar 800×820×350 mm; bukan ukuran produksi tervalidasi |
| Pengunjung | 8 pad tanam, 4×2; tahan = bunyi, lepas = mute |
| Prioritas | Satu pad aktif, pad pertama yang ditahan memiliki output |
| RGB | Standby putih; aktif pink/tosca; pulsa visual opsional |
| Servis | Tersembunyi, pintu belakang berkunci |
| Kontrol | 3 knob: FREQ, LEVEL, FOLD |
| Selector | WAVE SINE/SAW/TRIANGLE; HARMONICS ODD/EVEN/ODD+EVEN |
| Tombol | SAVE dan TEST/MUTE; mode RUN/SERVICE |
| Preset | Simpan frekuensi, level, wave, fold, harmonik ke pad tujuan |
| Sweep | Batas MIN/MAX tersimpan; nilai final menunggu uji pelat |

## Acuan komponen

ESP32-S3 DevKitC-1 N16R8, PCM5102 I²S DAC, MAX7219 quad 32×8, YS-502MT amplifier, MCP23017, SN74AHCT125N, dan WS2812B. Untuk panel: Bourns PEC11R-4215F-S0024, C&K OS103011MA7QP1 / OS102011MA1QN1, Omron B3F-1000.

Komponen belum dibeli pada keputusan blueprint. Cocokkan datasheet, unit aktual, terminal fisik, tegangan dan polaritas amplifier sebelum fabrikasi/solder. Ukuran MAX7219 acuan 128×32×14 mm tidak membuktikan semua modul pasar identik.

## Mekanik blueprint

Panel pengunjung 220×122×2 mm; servis 260×180×2 mm. Pad cap 40×40 mm, jendela 41×41/R2; frame, cap, spring, PCB carrier dan stopper masih custom. Box pad target 24 mm dalam celah 30 mm menyisakan sekitar 6 mm sebelum memperhitungkan pasir dan gerak pelat.

Template potong adalah acuan prototipe. SVG/CSV memakai datum kiri atas (Y turun), DXF kiri bawah (Y naik). Cetak A3 pada 100% dan ukur kotak kalibrasi 50×50 mm. Ukuran panel/cutout usulan harus dibedakan dari dimensi komponen pada datasheet.

## Kontrol simulator web — 26 Oktober 2026

Atas permintaan pengguna, jangkauan simulator menjadi **20–4000 Hz** (default MAX 4000), FOLD bergerak per **0,1%**, dan frekuensi dapat diketik dengan presisi **0,1 Hz** melalui PANEL SERVIS. Batas sweep yang pernah disimpan tetap dipertahankan; MAX dapat diperluas lewat SIMPAN SETUP. Tombol keyboard angka 1–8 serta Numpad 1–8 (Num Lock aktif/nonaktif) menahan pad sampai tombol dilepas; input angka tidak memicu pad. Ini bukan perubahan batas aman hardware atau blueprint.
