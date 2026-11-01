# Momentary pad / operator Wi-Fi - 1 November 2026

Permintaan pengguna: empat pad momentary, bunyi maksimal lima detik per tekan; preset awal 1096/1360/2249/2386 Hz. Pengguna kemudian secara eksplisit menetapkan password hotspot bersama agar mudah dibagikan ke operator.

## Implementasi

Controller menyimpan waktu awal dan ID setiap press; deadline unsigned 5000 ms tahan wrap. Setelah cutoff ownership tetap ditahan sampai release, sehingga event down/heartbeat/edit tidak memperpanjang durasi atau mengulang. Remote pad mendapat batas yang sama selain lease hold/sesi yang sudah ada. DSP membatasi satu ID press ke 240.000 sampel pada 48 kHz, fade 10 ms terakhir dan nol persis setelah batas, termasuk release tail. TEST/sequence/GATHER tidak terkena batas global ini.

Preset awal mengganti frekuensi empat contoh; wave/level/fold/harmonics lain tetap. NVS tersimpan tidak ditimpa. Pulihkan empat preset awal di HP mengganti slot secara eksplisit. Sweep tersimpan lama tidak diperluas otomatis.

Konfigurasi hotspot berada di firmware/include/service_config.h. Password pilihan pemilik dibundel sesuai instruksi eksplisitnya, menggantikan penggunaan password acak NVS lama. Tidak perlu erase; preset/sweep tetap tersimpan. API state/ekspor tidak memuat password. Token sesi masih acak. Nilai password sengaja tidak diulang pada bukti JSON.

## Verifikasi

- `sh firmware/test/run-host.sh --asan`: DSP/state lulus; 419 cek kontrol/protokol/NVS, nol gagal, ASan/UBSan/float-cast-overflow. Meliputi semua pad, release awal, boundary 4999/5000, no autorepeat, re-arm, wrap millis, SERVICE edits, heartbeat, factory/custom persistence. DSP dipaksa menerima sounding target >5 detik untuk membuktikan sample cutoff tanpa bantuan control loop, termasuk tail saat release menjelang deadline.
- `pio run -d firmware`: lulus tanpa warning; RAM 46.732 byte, flash aplikasi 862.405 byte. Build mencakup konfigurasi operator baru.
- `node --test simulator/tests/*.test.js domain-router/worker.test.mjs`: 44 lulus. `python3 scripts/verify_manifest.py`: 86 aset.
- Chrome/CUA preview C++ lokal: empat frekuensi benar, petunjuk 5 detik, klik/tahan native 6200 ms lalu release mute; pad 2 recall 1360; mobile 390x844 tanpa overflow; disconnect RUN/mute. Screenshot mobile-presets.png dan ui-checks.json. Ini bukan pengujian batas sampel audio; tes host yang membuktikannya.
- PDF Rev03 tujuh halaman dirender dan semuanya diperiksa, termasuk foto/pin/diagram dari Rev02. Preset, momentary dan petunjuk operator diperbarui. pdf-1.png sampai pdf-7.png.
- Paket v1.1.0: helper --check lulus; checksum binary/source, CRC ZIP, kesamaan PDF, dan penolakan binary corrupt lulus. Nilai konfigurasi Wi-Fi ditemukan pada hasil binary; tidak membuktikan koneksi radio nyata.

## Batas bukti

Belum di-flash atau diuji di ESP32 fisik. Belum membuktikan timing pad/GPIO/HP/radio/I2S/amplifier/shaker nyata. Simulator publik tetap historis dan tidak dideploy dalam pekerjaan ini. Rilis GitHub target firmware-remote-v1.1.0; source dan assets tetap privat.
