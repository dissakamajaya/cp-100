# Daftar kelanjutan

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
- [ ] Implementasi firmware ESP32, penyimpanan preset, boot mute dan gate release; belum ada firmware dalam repo.
- [ ] Rancang PCB/carrier/Gerber dan CAD produksi setelah prototipe teruji.

Urutan perangkat di atas merupakan pekerjaan lanjutan; paket ini tidak menyatakan desain siap produksi.
