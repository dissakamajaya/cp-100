# CP-100 — firmware remote Wi-Fi

Revisi minimal 1 November 2026: **empat pad horizontal**, ESP32-S3 N16R8, FREQ **20–4000 Hz / 0,1 Hz**, FOLD **0–100% / 0,1%**. SERVICE sekarang memakai aplikasi web lokal di HP. Encoder, slide switch dan MCP23017 panel servis lama tidak diperlukan. Display MAX7219, LED eksternal WS2812B dan buffer AHCT125 juga tidak diperlukan. Hardware minimum: ESP32-S3, empat tombol pad, PCM5102 dan amplifier/shaker.

## Memakai panel HP

1. Upload firmware melalui port **UART** pada DevKit, lalu buka monitor serial 115200.
2. Password hotspot bersama: **300300300**, sesuai pilihan pemilik untuk operator. Firmware memakai konfigurasi ini langsung, menggantikan password acak lama tanpa menghapus preset/NVS. Ketik `wifi` untuk melihat info koneksi. Konfigurasi ada di `include/service_config.h`; password tidak dimasukkan ke API state atau ekspor preset.
3. Sambungkan HP ke Wi-Fi **ATMOS-CP-300-EXP**. Pilih tetap tersambung jika HP mengatakan jaringan tidak memiliki internet.
4. Buka **http://192.168.4.1** di browser HP, lalu tekan **Hubungkan kontrol**. Tidak perlu internet, akun, aplikasi terpasang, atau koneksi router.
5. Perangkat masuk SERVICE dalam keadaan **mute**. Atur FREQ, LEVEL, FOLD, WAVE dan HARMONICS. Geser knob ke atas/bawah, gunakan tombol ±, atau ketik angka. **TEST SUARA** membunyikan suara yang sedang diedit.
6. **SIMPAN 1–4** menyimpan suara ke pad tujuan dan mematikan output. Pad fisik dan preview pad di HP: tahan untuk bunyi, lepas untuk mute, maksimal **5 detik per tekan**. Setelah timeout harus dilepas lalu ditekan ulang, tanpa autorepeat. Pad fisik tetap memakai prioritas pad pertama yang ditahan sampai dilepas.
7. **Selesai servis** mengembalikan perangkat ke RUN dan mute. Untuk pengunjung, pad fisik beroperasi tanpa HP.

Tombol **MUTE / STOP** selalu tersedia di bagian bawah layar. Hanya satu sesi kontrol dapat aktif. Tab ditutup, HP tidur atau koneksi hilang: sesi kedaluwarsa setelah **2,5 detik**, playback berhenti, perangkat kembali RUN dan mute. Hold pad remote memiliki batas tambahan **1,2 detik** tanpa pembaruan hold. Tidak ada auto-resume saat tersambung kembali. Physical MUTE opsional tetap berfungsi.

Hotspot memakai HTTP lokal melalui jaringan Wi-Fi berpassword. Alamat dibuka manual; firmware ini tidak mengandalkan captive portal, mDNS, atau internet. Dua perangkat dapat bergabung ke hotspot, tetapi hanya satu sesi dapat mengubah pengaturan. Jangan menjalankan dua rakitan dengan SSID yang sama di tempat yang sama tanpa penamaan jaringan tersendiri.

## Preset awal

PAD 1 **1096 Hz**, PAD 2 **1360 Hz**, PAD 3 **2249 Hz**, PAD 4 **2386 Hz**. Wave/level/fold/harmonics selain frekuensi tetap seperti versi sebelumnya. Nilai ini dipakai pada NVS baru atau saat memilih **Preset awal -> Pulihkan empat preset awal** di HP. Upgrade tidak menimpa preset tersimpan. Batas sweep lama tetap berlaku; bila MAX masih 2000 Hz, pad 3/4 diblokir sampai batas diperluas dan diuji pada rakitan.

Batas 5 detik menggunakan deadline `millis()` yang tahan wrap dan batas **240.000 sampel pada 48 kHz** di DSP, dengan fade 10 ms di akhir sebelum cutoff. Heartbeat, drag knob, dan event down duplikat tidak mengulang budget. TEST/sequence/GATHER tetap memakai aturan playback masing-masing.

## Fitur panel

| Kontrol | Perilaku |
|---|---|
| RUN / SERVICE | Semua perubahan suara/setup hanya di SERVICE; perpindahan mode mematikan output |
| FREQ / LEVEL / FOLD | 0,1 Hz; LEVEL integer 0–100%; FOLD 0,1% |
| WAVE | SINE / SAW / TRIANGLE |
| HARMONICS | ODD / EVEN / ODD+EVEN |
| Empat preset | Simpan, preview hold/release, impor/ekspor JSON, pulihkan empat contoh |
| SWEEP MIN / MAX | Batas tersimpan untuk editing dan recall preset; tidak menghapus preset di luar batas |
| Sequencer | Empat slot, 3 detik/slot, urut atau acak; slot kosong/di luar sweep tetap diam |
| GATHER | Editor 1–16 langkah: Hz, LEVEL, GLIDE, HOLD; maksimal 300 detik, sine-only, fade akhir 2 detik; simpan rutin di perangkat |
| Diagnostik | Beban render audio dan penghitung stall I2S |

Input pad manual, edit parameter, perubahan mode/setup, MUTE, atau sesi terputus membatalkan playback. Sebuah pad fisik yang sedang ditahan tidak dapat direbut oleh pad remote atau perintah TEST/sequence/GATHER. Preset contoh dan GATHER belum dikalibrasi pada pelat nyata; GATHER tidak dijalankan otomatis saat idle.

## Build dan upload

**Paket siap flash:** [remote-v1.1.0](releases/remote-v1.1.0/README.md) atau [ZIP binary + PDF](releases/CP-100-remote-v1.1.0.zip). **PDF wiring terbaru:** [Rev04: foto DevKit dan diagram kabel, tanpa display/LED](../output/pdf/CP-100-Wiring-4-Pad-WiFi.pdf). Paket binary berisi empat image terpisah dan checksum; tidak menulis padding ke NVS.

PlatformIO, `espressif32@6.9.0` / Arduino 2.0.17, flash QIO 16 MB, PSRAM OPI. Satu dependency terpin: ArduinoJson 6.21.5. HTTP server memakai ESP-IDF bawaan. Asset HTML/CSS/JS lokal di `web/` dikompresi gzip dan di-embed otomatis oleh `tools/embed_web.py`; tidak perlu upload filesystem terpisah.

```sh
gh repo clone dissakamajaya/cp-100
cd cp-100
pio run -d firmware
pio run -d firmware -t upload
pio device monitor -d firmware
```

Audio berjalan di core 1 dengan prioritas di atas loop kontrol. Wi-Fi dan HTTP berada di core 0. Request HTTP diteruskan ke loop melalui antrean terbatas, sehingga pembacaan jaringan tidak memblokir polling pad. Perintah yang belum mulai diproses dalam 250 ms ditolak. Operasi penyimpanan mematikan output dan mengosongkan DMA sebelum menulis NVS. Kegagalan tulis ditampilkan sebagai **RAM saja**, bukan sukses tersimpan.

## Migrasi penyimpanan

- Boot selalu RUN dan mute.
- Layout NVS v1 (delapan pad / FOLD integer) dibaca sebagai empat slot pertama. Data lama tidak ditimpa saat boot; perubahan baru memakai layout v2.
- Batas sweep lama, termasuk MAX 2000 Hz, tetap dipertahankan. MAX 4000 adalah kemampuan firmware/default perangkat baru, bukan perluasan otomatis batas rakitan lama.
- Impor file empat slot didukung. File lama delapan slot harus dikonfirmasi di UI; hanya slot 1–4 yang dipakai, dan seluruh file divalidasi sebelum perubahan.
- Ekspor berisi empat preset (`chladni-08`, version 4, `parity-fold-v2`). Simulator publik yang masih delapan slot belum dapat mengimpor ekspor empat slot ini.
- Password Wi-Fi tidak disertakan dalam API state atau ekspor preset.

## Wiring saat ini

| Modul | Sambungan ESP32-S3 |
|---|---|
| Pad 1–4 | GPIO1, GPIO2, GPIO4, GPIO5; masing-masing switch ke GND, pull-up 10k ke 3V3 |
| MUTE fisik opsional | GPIO16 ke GND, pull-up 10k ke 3V3; kini hanya mematikan suara |
| PCM5102 BCK / LCK / DIN | GPIO18 / GPIO21 / GPIO47 |
| PCM5102 SCK | GND; FLT/DEMP/FMT LOW, XSMT HIGH (GY-PCM5102: jumper 1L, 2L, 3H, 4L); cocokkan silkscreen modul aktual |

Semua GND terhubung. Pin 6–9 tidak lagi dipakai pad. Encoder FREQ/LEVEL/FOLD, SAVE fisik, selector MODE/WAVE/HARMONICS dan MCP23017 lama tidak dibaca oleh aplikasi saat ini; jangan gunakan pinout PDF Rev08 delapan pad sebagai wiring revisi ini. Driver panel/display/LED lama disimpan untuk riwayat dan dikecualikan dari build. GPIO38-41 tidak digunakan untuk output display/LED. Pelat 600×600 mm, kaca Ø500 mm, enclosure pad 220×74×24 mm tetap.

Level audio masih memakai trim hardware **0,5** (sekitar 0,18 RMS full-scale digital pada LEVEL 100%). Browser simulator terbaru tidak lagi memiliki attenuasi 0,12. Kesamaan parameter tidak menyatakan tegangan atau volume fisik sama.

## Verifikasi tanpa board

```sh
pio pkg install -d firmware
sh firmware/test/run-host.sh
sh firmware/test/run-host.sh --asan
node --test simulator/tests/*.test.js domain-router/worker.test.mjs
python3 scripts/verify_manifest.py
```

Runner host portable macOS/Linux menguji DSP, semua 9.009 entri kalibrasi FOLD, state empat pad, sesi/timeout, ownership, sequence, GATHER, validasi JSON dan migrasi penyimpanan. `test/fakes/Preferences.h` hanya mengganti transport NVS dengan memori. Suite lama `test/sim/` adalah referensi Rev08 delapan pad dan panel fisik; **bukan suite aktif revisi remote**.

Preview UI lokal memakai mesin C++ yang sama, tanpa radio/DAC/GPIO:

```sh
c++ -std=c++17 -O2 -Ifirmware/src -Ifirmware/test/fakes -Ifirmware/.pio/libdeps/cp100/ArduinoJson/src firmware/test/preview_driver.cpp firmware/src/controller.cpp firmware/src/model.cpp firmware/src/dsp.cpp firmware/src/protocol.cpp firmware/src/storage.cpp -o /tmp/cp100-remote-preview
node firmware/test/preview-server.mjs
```

Buka `http://127.0.0.1:8765`. Ini pengujian UI/control engine, bukan emulasi ESP-IDF HTTP atau hardware. Penyimpanan preview di memori dan hilang saat proses dihentikan.

## Uji hardware berikutnya

1. ESP32 saja: upload, `wifi`, akses panel HP, cek boot mute, disconnect/reconnect dan sesi kedua.
2. Tambahkan PCM5102 ke speaker aktif berlevel rendah; cek TEST, MUTE, hold/release dan perubahan cepat kontrol sambil memantau stall I2S.
3. Tambahkan empat pad dan MUTE fisik opsional. Cek penekanan bersamaan, satu siklus sequencer, dan putus Wi-Fi saat TEST/sequence/GATHER.
4. Matikan/nyalakan perangkat, verifikasi preset, sweep, rutin GATHER serta password operator tetap berlaku, tetapi output tetap mute.
5. Baru sambungkan amplifier/shaker/pelat; tentukan batas sweep dan level pada rakitan nyata.

Build/host/UI QA tidak membuktikan timing Wi-Fi+audio pada ESP32, kestabilan koneksi HP, level DAC, atau keberhasilan mengumpulkan pasir. Bukti dan hasil terkini: `../evidence/momentary-2026-11-01/`; riwayat QA remote lengkap: `../evidence/remote-service-2026-10-31/`.
