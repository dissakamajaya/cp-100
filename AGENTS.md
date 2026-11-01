# Instruksi agent CP-100

Mulai dengan README.md, HANDOFF.md, docs/DECISIONS.md dan docs/NEXT-STEPS.md. Pertahankan keputusan terbaru: 4 pad horizontal, pelat 600×600 mm, kaca Ø500 mm, SERVICE lewat HP via hotspot ESP32 ATMOS-CP-300-EXP (192.168.4.1), tanpa display/LED eksternal atau panel servis fisik. Pad momentary maksimal 5 detik per tekan tanpa autorepeat; preset awal 1096/1360/2249/2386 Hz. Password hotspot bersama dipilih pemilik di firmware/include/service_config.h, menggantikan random NVS lama. Jangan menganggap PDF Rev08 awal sudah memenuhi koreksi gambar terakhir.

Sebelum mengedit, periksa branch/status Git dan hasil tes. Jangan revert perubahan pengguna/agent lain. Update HANDOFF dan bukti verifikasi setelah perubahan yang relevan. Jangan menyimpan credential atau menerbitkan foto/referensi sebagai aset bebas lisensi.

## Browser automation

Sesuai instruksi pengguna 31 Oktober 2026, Stagehand tidak lagi diwajibkan. Gunakan tool browser yang tersedia untuk QA dan simpan bukti. Native service connectors/CLI boleh digunakan untuk operasi non-browser GitHub/Sites/Cloudflare.

## Verification

```sh
sh firmware/test/run-host.sh --asan
pio run -d firmware
node --test simulator/tests/*.test.js domain-router/worker.test.mjs
python3 scripts/verify_manifest.py
```

Tes simulator bukan bukti hardware. QA historis bukan bukti live terbaru. Push GitHub bukan deployment Sites. Selesaikan koreksi visual, render PDF dan periksa output sebelum publikasi.
