# Rilis minimal 1 November 2026

Empat pad + SERVICE lewat HP; tanpa panel servis fisik, display atau LED eksternal. MUTE GPIO16 opsional. Source commit `74285937b2fbeeefb9bb8b75c385289c8ca334eb` telah di-push ke main dan tag `firmware-remote-v1.0.0`, repository privat. Rilis: https://github.com/dissakamajaya/cp-100/releases/tag/firmware-remote-v1.0.0. Ref main/tag serta digest SHA256 ZIP/PDF pada aset GitHub cocok dengan file lokal.

## Verifikasi

- `sh firmware/test/run-host.sh --asan`: DSP/state lulus; 326 cek kontrol/protokol/NVS, 0 gagal. ASan/UBSan/float-cast-overflow. 9.009 entri kalibrasi dan 72 kombinasi render/release diuji. Lihat `host-tests.txt`.
- `pio run -d firmware`: sukses tanpa warning, RAM 46.724 byte, flash aplikasi 862.341 byte. Lihat `build.txt`.
- `node --test simulator/tests/*.test.js domain-router/worker.test.mjs`: 44 lulus, lihat `simulator-router-tests.txt`.
- `python3 scripts/verify_manifest.py`: 86 aset sumber valid, lihat `manifest.txt`.
- `python3 firmware/releases/remote-v1.0.0/flash.py --check`: empat hash/offset valid. ZIP CRC valid, PDF ikut paket, binary yang sengaja diubah ditolak. Lihat `flash-package-checks.json`.
- UI Chrome melalui CUA di preview lokal `127.0.0.1:8765`, mesin C++ yang sama dengan fake NVS: connect mute, empat pad, tidak ada input LED, setup MIN/MAX tersimpan, FREQ 256,7/TEST aktif, MUTE, viewport 390×844 tanpa overflow, restore MAX 4000, disconnect RUN/mute. Lihat `ui-checks.json`, `mobile-controls.png`, `mobile-minimal.png`. Server dimatikan setelah QA.
- PDF empat halaman dirender melalui Poppler dan seluruh halaman diperiksa: tidak ada clipping/overlap/header hilang. Salinan render `wiring-1.png` s.d. `wiring-4.png`; builder `scripts/build_wiring_guide.py` (ReportLab). Hash final di `results.json`.

Paket flash dibuat oleh `scripts/package_firmware.py`. Empat image terpisah menghindari padding pada region NVS; hash source dan binary dicatat pada manifest rilis. Tidak ada credential Wi-Fi per perangkat disimpan dalam repo.

## Batas bukti

Tidak ada upload atau uji ESP32 nyata. Preview tidak meniru ESP-IDF HTTP, radio, GPIO, I2S, amplifier, shaker atau pelat. Koneksi HP/hotspot, runtime audio dan kalibrasi hardware masih perlu diuji. PDF lama/simulator publik tetap historis; push repo bukan deployment Sites. Enclosure/STL terdahulu disertakan tanpa klaim uji cetak.
