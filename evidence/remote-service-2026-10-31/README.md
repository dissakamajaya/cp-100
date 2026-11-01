# Firmware empat pad + SERVICE HP

Implementasi dimulai 31 Oktober 2026 dan verifikasi akhir diselesaikan **1 November 2026 (Asia/Jakarta)**. Pekerjaan lokal; belum di-commit, di-upload ke ESP32, atau diuji pada hardware fisik.

## Hasil

- Firmware empat pad: GPIO1/2/4/5, empat preset dan WS2812B, FREQ 20–4000 Hz per 0,1 Hz, FOLD per 0,1%.
- SERVICE pindah dari panel fisik ke aplikasi HP di hotspot **ATMOS-CP-300-EXP**, alamat **http://192.168.4.1**. Password acak per perangkat disimpan di NVS; operator membaca lewat serial `wifi`. Tidak ada credential di repo atau API state.
- Semua knob, WAVE/HARMONICS, TEST/MUTE, SAVE per slot, impor/ekspor, setup sweep/LED, sequence empat slot/3 detik urut/acak, dan GATHER manual tersedia offline.
- Satu pemilik sesi; timeout 2,5 detik kembali RUN/mute, hold remote timeout 1,2 detik. Perintah tertunda kedaluwarsa setelah 250 ms. Tidak ada auto-resume.
- Audio dan mutasi state dipisahkan dari HTTP. Audio core 1, Wi-Fi/HTTP core 0; queue command dan snapshot terproteksi. Statistik audio memakai atomic. Input jaringan yang lambat tidak menahan loop pad.
- Penyimpanan mematikan suara dan mengosongkan DMA sebelum NVS ditulis. UI membedakan hasil tersimpan dan RAM saja. Layout v1 delapan pad dibaca sebagai empat slot pertama; batas sweep lama tidak diperluas otomatis.
- Knob mengirim nilai terbaru secara berkala, maksimal satu adjustment request dalam proses. MUTE membatalkan adjustment/perintah yang masih menunggu, termasuk kelanjutan TEST.
- Driver panel fisik lama disimpan sebagai riwayat dan dikecualikan dari build. Pad fisik dan MUTE opsional GPIO16 tetap berfungsi. Tidak ada perubahan pelat 600×600 mm/kaca Ø500 mm/enclosure empat pad.

Petunjuk operator, wiring dan upload: [firmware/README.md](../../firmware/README.md).

## Verifikasi

| Pemeriksaan | Hasil |
|---|---|
| ESP32-S3 PlatformIO | PASS, tanpa warning; RAM 47.732 byte (14,6%), flash aplikasi 884.429 byte (13,5%) |
| DSP/state host | PASS; 9.009 entri kalibrasi FOLD; 72 kombinasi render/release pada 20/4000 Hz, tiga wave dan harmonik, termasuk FOLD pecahan |
| Kontrol/protokol/NVS host | **323 cek PASS, 0 gagal**, dengan ASan/UBSan/float-cast-overflow |
| UI browser | **13 cek PASS** pada preview lokal; viewport 390×844 dan lebar 320 px |
| Simulator + router | **44 PASS**, 0 gagal |
| Manifest paket lama | **86 aset PASS**; ini tidak mencakup source firmware baru |
| Serial USB | Tidak ada perangkat dengan USB VID/PID terdeteksi pada pemeriksaan akhir |

Command dari root `/Users/0xtx/Documents/GitHub/cp-100`:

```sh
pio pkg install -d firmware
sh firmware/test/run-host.sh --asan
node --check firmware/web/app.js
pio run -d firmware
node --test simulator/tests/*.test.js domain-router/worker.test.mjs
python3 scripts/verify_manifest.py
git diff --check
pio device list --json-output
```

Sebelumnya full toolchain build juga lulus; build terakhir mengompilasi ulang source/asset yang berubah. Ringkasan, hash binary dan source ada di [results.json](results.json). Log mentah lokal `build.log`, `host-tests.log`, `simulator-router-tests.log`, `manifest.log`, `dependencies.log` diabaikan oleh aturan global Git `*.log`; ringkasan JSON/Markdown disimpan terpisah.

## Bukti UI

Preview menggunakan **controller, protocol, dan codec storage C++ produksi**, dengan transport HTTP Node loopback dan fake NVS di memori. Tidak ada radio, GPIO, I2S, DAC, amplifier, atau pelat yang diemulasi. Diagnostik audio 0% pada preview bukan pengukuran beban ESP32.

Command preview:

```sh
c++ -std=c++17 -O2 -Ifirmware/src -Ifirmware/test/fakes -Ifirmware/.pio/libdeps/cp100/ArduinoJson/src firmware/test/preview_driver.cpp firmware/src/controller.cpp firmware/src/model.cpp firmware/src/dsp.cpp firmware/src/protocol.cpp firmware/src/storage.cpp -o /tmp/cp100-remote-preview
node firmware/test/preview-server.mjs
```

Browser QA memakai tool browser yang tersedia sesuai instruksi pengguna bahwa Stagehand tidak lagi diwajibkan. Flow: connect → 4000 Hz/FOLD 18,1%/LEVEL → TEST → SAVE → RUN (kontrol disabled) → SERVICE → sequence → MUTE → edit dan jalankan GATHER sampai fade/mute → simpan sweep/LED → disconnect. Kelanjutan 1 November: drag FREQ 128,5→228,5 Hz, perubahan cepat/TEST/MUTE, serta cek overflow 320 px. Override viewport dikembalikan setelah pengujian; sesi servis diakhiri.

- [10 pemeriksaan awal](browser-checks.json)
- [3 pemeriksaan akhir](browser-continuation.json)
- [Screenshot HP akhir](mobile-panel-final.png)
- [Screenshot GATHER](mobile-gather.png)

## Batas dan pekerjaan berikutnya

- Belum ada upload atau uji firmware pada ESP32 nyata, termasuk coexistence Wi-Fi/audio, latency pad, actual NVS/reboot, orientasi matrix, jumper DAC dan LED fisik.
- Tes host menggunakan transport NVS palsu; uji migrasi codec tidak membuktikan perilaku flash fisik. Preview HTTP Node tidak memverifikasi HTTP server ESP-IDF/radio.
- Suite `firmware/test/sim/` adalah harness historis delapan pad/panel fisik dan bukan suite revisi ini. Tidak mengklaim ulang 218 cek historis atau QEMU.
- Sweep/level akhir dan frekuensi GATHER harus dikalibrasi pada amplifier/shaker/pelat nyata. Tidak ada GATHER otomatis saat idle.
- Simulator publik masih delapan slot; ekspor empat slot dari remote belum kompatibel dengan importer publik tersebut. PDF/pinout/diagram Rev08 juga perlu sinkronisasi tersendiri. Tidak ada publikasi Sites/Cloudflare dalam perubahan ini.
