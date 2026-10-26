# Sumber dan provenance

Snapshot disiapkan 26 Oktober 2026, Asia/Jakarta.

## Konteks utama

- Chat Codex: `codex://threads/01a15d32-d71c-77e1-bce0-aead509b9348`.
- Folder sumber: `/Users/0xtx/Documents/Codex/2026-10-21/maaf-baru-bangun-untuk-audio-source`.
- Diskusi tambahan: https://chatgpt.com/share/6adedb0c-ff7c-83ec-8d88-d171cdc78917 (judul terlihat: Chladni Plate Specification).

Chat Codex dibaca langsung untuk instruksi terakhir. Shared chat dibuka melalui Stagehand. Bagian yang terlihat memuat opsi awal keyboard MIDI 25-key/100 cm, kemudian catatan prototipe speaker full-range 8 inci, preamp 3-way, sweep Ableton, ketebalan perkiraan 0,8 mm, dan pola >1 kHz. Terdapat ukuran prototipe yang ambigu (0,8 m dan sekitar Ø20 cm); jangan jadikan itu spesifikasi final CP-100. Lampiran media shared chat tidak diunduh atau dianalisis ulang dalam packaging.

## Hosting yang sudah ada

- Sites project ID: `appgprj_6ad8a720c7bc81919ed7d22134c3c6aa`.
- Upstream: https://chladni-08-lab.vocallime128.chatgpt.site
- Custom path: https://houseofexp.com/research/cp-100/
- Worker: `cp-100-research-route`, binding `WEBSITE` → `house-of-exp`.
- Konfigurasi route: `domain-router/wrangler.json`.

ID project/account/zone bukan kredensial. Token/credential, Git remote Sites, `.openai`, cache, dependensi terpasang dan data sesi tidak disalin. Bila perlu publikasi, peroleh credential baru melalui tool Sites resmi. Jangan menggunakan repo GitHub ini sebagai pengganti remote Sites secara diam-diam.

## File terpilih

`SOURCE-MANIFEST.json` mencatat hash file asal dan hasil packaging. Source simulator berasal dari `work/sites/chladni-08`, bukan `outputs/chladni-simulator` yang lebih lama. PDF Rev07 disertakan karena builder Rev08 mempertahankan tiga halaman wiring dari PDF tersebut. Cover pilihan dan dua gambar casing asli disertakan.

Hanya dua jenis adaptasi portability dilakukan: path foto dalam manifest menjadi relatif terhadap root repo, dan fungsi `image_fit` pada builder Rev08 me-resolve path relatif terhadap root repo. Isi desain, source simulator, router, PDF dan blueprint yang disalin tetap sama.

Foto komponen dan sumbernya: `work/component-photos/sources.json`, `work/rev8/photo-sources.json`. Datasheet lokal: `work/rev8/sources/`. Sumber dimensi juga tertaut dalam PDF.

## Penanganan versi

PDF Rev08 awal: SHA-256 `b3e7737a3d976300a234f36f12e0768f8040ac7b33e8340b50d6c048138e7f78` menurut catatan historis; hash aktual paket tercantum pada manifest. Rebuild bisa berbeda secara byte karena metadata PDF. Periksa isi dan visual; jangan mengklaim hash baru membuktikan desain fisik sudah teruji.

File lama yang tidak diperlukan untuk rebuild atau handoff tidak disalin. Folder sumber tetap utuh sebagai arsip lokal.
