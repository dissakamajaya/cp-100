# CP-100

Eight momentary RGB pads, internal service controls, three knobs, two switches, stored sweep limits, continuous audio and sand preview.

Web controls (26 October 2026): default frequency range **20–4000 Hz**, FOLD in **0.1%** steps, and a typed frequency field (**0.1 Hz**) in PANEL SERVIS. Type a value and press Enter or ATUR. Saved sweep limits still apply; to extend an existing 2000 Hz ceiling, set MAX / Hz to 4000 and choose SIMPAN SETUP.

Hold keyboard digits **1–8** or **Numpad 1–8** to play the corresponding saved pad; release to mute. Numpad shortcuts also work with Num Lock off. Number entry fields keep normal typing. Empty pads remain silent until a preset is saved. These are simulator controls, not commissioned hardware limits.

The sequencer sits below the pads: choose **1 → 8** or **ACAK / RANDOM**, then press **PLAY**. It enables audio, plays each slot for **3 seconds**, and loops. Random mode shuffles all eight slots once per cycle without consecutive repeats across cycles. Empty or excluded slots stay silent. **STOP**, manual pad input, mode/setup/parameter changes, audio-off, mute, and leaving the page stop sequencing; it never resumes automatically.

**KUMPULKAN PASIR / GATHER** (28 Oktober 2026) ada di PANEL SERVIS: rutin sine sekali-jalan untuk mengumpulkan pasir kembali ke tengah kaca. Setiap langkah punya Hz, LEVEL, GLIDE (detik, frekuensi bergeser eksponensial dari langkah sebelumnya) dan HOLD (detik). Setelah langkah terakhir level fade 2 detik lalu mute. Maksimal 16 langkah / 300 detik; tersimpan di browser (`chladni-08-gather-v1`). Contoh awal belum dikalibrasi: cari frekuensi yang membentuk ring di dalam kaca pada pelat nyata, urutkan dari ring terbesar ke terkecil dengan level menurun, lalu tambahkan lewat **+ DARI FREQ**. Pad, sequence, mute, perubahan mode/setup/parameter dan meninggalkan halaman menghentikan GATHER. Belum ada di firmware.
