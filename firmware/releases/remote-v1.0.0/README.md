# CP-100 remote v1.0.0 - wiring minimal

Untuk **ESP32-S3 N16R8 (16 MB flash / 8 MB OPI PSRAM)**. Empat pad; SERVICE lewat HP. **Tanpa MAX7219, LED eksternal, AHCT125 atau MCP23017.** Firmware bench: build/host/UI diuji, hardware fisik belum diuji.

## Cara flash binary

1. Unduh ZIP rilis lalu ekstrak. Atau clone repo dan masuk ke `firmware/releases/remote-v1.0.0`.
2. Lepas amplifier/shaker saat flashing. Sambungkan kabel USB data ke port board **UART**. Hindari memasukkan daya USB dan 5 V eksternal ke board secara bersamaan.
3. Buat virtual environment Python, lalu jalankan dari folder ini:

```sh
python -m pip install -r requirements.txt
python flash.py --check
python flash.py --port COM5
```

Ganti `COM5` dengan port aktual. Linux sering `/dev/ttyUSB0`; macOS sering `/dev/cu.usbserial-...`. Bila koneksi tidak stabil, tambahkan `--baud 115200`. Bila perlu download mode: tahan BOOT, tekan/lepas RESET, lalu lepas BOOT.

`flash.py` memvalidasi SHA256 sebelum mengakses board. **Tidak menjalankan erase_flash**, dan tidak menulis image pada region NVS `0x9000-0xDFFF`. Namun bootloader, partition table, OTA initializer dan aplikasi akan diganti. Gunakan hanya pada board/partition layout CP-100 yang sesuai; simpan backup sebelum mengganti firmware perangkat lain.

| Offset | File |
|---|---|
| 0x0000 | bootloader.bin |
| 0x8000 | partitions.bin |
| 0xE000 | boot_app0.bin |
| 0x10000 | firmware.bin |

Jangan menulis `firmware.bin` sendirian ke 0x0. Tidak ada merged image berisi padding yang berpotensi menimpa NVS. Tidak diperlukan upload filesystem karena UI HP sudah tertanam di aplikasi.

## Sesudah flash

Buka monitor serial **115200** menggunakan PlatformIO (`pio device monitor -d firmware` dari root repo) atau aplikasi terminal serial. Ketik `wifi` untuk melihat password per perangkat. Sambungkan HP ke **ATMOS-CP-300-EXP**, buka **http://192.168.4.1**, lalu Hubungkan kontrol. Awalnya mute. Jangan memasukkan password tersebut ke GitHub.

PDF wiring berada di `output/pdf/CP-100-Wiring-4-Pad-WiFi.pdf` pada repo dan ikut disertakan dalam ZIP rilis. GPIO pad: **1, 2, 4, 5**; DAC BCK/LRCK/DIN: **18, 21, 47**; MUTE fisik opsional: **16**. Kontrol display dan LED tidak diinisialisasi pada build ini.

## Build dari source

Dari root repo:

```sh
pio run -d firmware -t upload
pio device monitor -d firmware
```

Build terpin di `firmware/platformio.ini`. `manifest.json` mencatat hash bagian binary dan hash kumpulan source firmware. Nama rilis di GitHub menunjuk commit source terkait.
