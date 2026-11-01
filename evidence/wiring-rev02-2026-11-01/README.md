# Wiring Rev02 - 1 November 2026

Koreksi permintaan pengguna: PDF harus memiliki gambar ESP32 DevKit dan diagram yang menunjukkan kabel dicolok ke mana.

PDF kini tujuh halaman: ringkasan/BOM; foto DevKit N16R8 dengan lingkaran/leader ke pin; diagram header fisik 22+22 pin ke empat tombol dan MUTE opsional; diagram DevKit ke terminal PCM5102 serta foto modul; tabel pin/rangkaian NO; detail DAC/daya/amplifier; flash/pengujian/sumber.

- Sisi komponen menghadap pembaca, antena di atas, USB di bawah. J1 kiri/J3 kanan dan semua pin aktif dicocokkan dengan tabel Espressif serta firmware/include/pins.h.
- Diagram/label adalah vektor ReportLab di atas foto referensi yang telah ada di work/component-photos/. Tidak memakai gambar hasil generasi untuk penempatan pin. Sumber dan hak foto tetap tercantum di PDF, tidak berlisensi bebas.
- Sumber primer: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html (tabel J1/J3, diakses 1 November 2026).
- Builder: scripts/build_wiring_guide.py + scripts/wiring_diagrams.py. ReportLab pada bundled Python; seluruh tujuh halaman dirender via `pdftoppm -scale-to 1400 -png` dan diperiksa. Bukti page-1.png sampai page-7.png. Label pin/kabel terbaca, tidak ada clipping/overlap.
- Pypdf memverifikasi tujuh halaman, nomor halaman dan dua foto tertanam. Periksa results.json untuk mapping header dan hash final.
- ZIP diperbarui dengan PDF final; CRC, checksum bagian binary dan kesamaan PDF dalam ZIP lulus. Semua binary/offset/source firmware sama dengan rilis firmware-remote-v1.0.0; dokumentasi Rev02 dan metadata PDF ditambahkan.
- Baseline sebelum edit: `sh firmware/test/run-host.sh --asan` lulus (326 cek, 0 gagal); Node simulator/router 44 lulus; manifest 86 aset lulus. Tidak ada perubahan kode firmware; tidak ada upload/uji hardware atau deployment Sites.

Batas: foto referensi bukan foto rakitan klien. Cocokkan label board/modul aktual, terutama clone; diagram terminal DAC disederhanakan dan tidak menyatakan semua breakout memiliki layout identik. VIN 5 V hanya bila breakout ber-regulator mengizinkan. Daya eksternal dan USB tidak digabung.
