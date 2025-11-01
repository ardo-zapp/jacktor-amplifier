# Jacktor Audio Monorepo

Jacktor Audio adalah ekosistem amplifier pintar berbasis ESP32 lengkap dengan panel bridge, aplikasi desktop (Electron), serta aplikasi Android (Compose) untuk kontrol dan telemetri. Repositori ini menampung seluruh komponen tersebut dalam satu monorepo.

## Struktur Repository

```
firmware/
  amplifier/    # Firmware ESP32 untuk unit amplifier utama
  panel/        # Firmware ESP32 panel bridge (USB OTG + UART bridge)
  partitions/   # Tabel partisi OTA bersama (jacktor_audio_ota.csv)
panel-ui/
  desktop/      # (WIP) Aplikasi desktop Electron + React
  android/      # (WIP) Aplikasi Android (Compose, target Android 16)
```

Masing-masing subdirektori memiliki README yang mendokumentasikan GPIO, fitur, dan instruksi build spesifik.

## Jalur Update Firmware

| Perangkat   | Mode OTA                                                                 | Mode Flash Langsung                           |
|-------------|---------------------------------------------------------------------------|-----------------------------------------------|
| Jacktor Audio Amplifier | Via panel bridge (UART2) menggunakan perintah `ota_*` line-based JSON.   | Colok micro-USB amplifier langsung dan flash memakai esptool/PlatformIO. |
| Jacktor Audio Panel Bridge | Via USB CDC/Android OTG memakai perintah `panel ota *` (CLI/JSON).       | Tekan BOOT + EN pada board panel dan flash melalui port USB panel.        |

- Kedua firmware memakai **file partisi yang sama**: `firmware/partitions/jacktor_audio_ota.csv` (layout dual-slot OTA + NVS). Hal ini menyederhanakan distribusi dan rollback karena image amplifier maupun panel memiliki ukuran maksimum yang sama.
- Selama OTA panel berlangsung, mesin OTG serta bridge UART dipause sehingga update tidak terganggu.
- OTA amplifier dijembatani panel tanpa kebutuhan jalur RX0/TX0 fisik; semua update rilis dilakukan via panel atau USB langsung pada perangkat amplifier.

## Dokumentasi Lanjutan

- `firmware/amplifier/README.md` — Fitur lengkap Jacktor Audio Amplifier, skema telemetri (termasuk `rtc_c` dan blok `features{}`), kebijakan OTA & RTC, serta langkah update.
- `firmware/panel/README.md` — Mapping GPIO final Jacktor Audio Panel Bridge, state machine OTG adaptif, fallback power, handshake, perintah CLI panel, dan instruksi OTA panel.
- `panel-ui/desktop/README.md` & `panel-ui/android/README.md` — Roadmap UI, dependensi build, dan spesifikasi CLI→JSON.

Kontribusi dipersilakan. Ajukan issue atau pull request untuk diskusi lebih lanjut.

### 🧰 PlatformIO + CLion Quick Setup (SAFE)

Repo ini sudah menyertakan dua proyek PlatformIO:
- `./firmware/amplifier`
- `./firmware/panel`

Jalankan skrip berikut untuk menyiapkan metadata CLion tanpa mengubah `platformio.ini`:
```bash
python tools/setup_pio_clion.py
```

Atau spesifik:
```bash
python tools/setup_pio_clion.py --projects ./firmware/amplifier ./firmware/panel
```

Secara default skrip tidak membuat/menimpa platformio.ini.
Jika Anda membuat proyek PlatformIO baru (di luar dua folder di atas), gunakan:
```bash
python tools/setup_pio_clion.py --projects ./path/proyek-baru --force-init
```

Ini hanya akan menjalankan `pio project init` jika `platformio.ini` belum ada.
Skrip ini mendeteksi lokasi repo secara otomatis, jadi Anda bisa menjalankannya dari root repo maupun direktori `tools/`.

### 🎨 UI Design (Landscape, Theme Blue)

Antarmuka panel (desktop & Android) mengusung tata letak **landscape** dengan grid 12 kolom, palet neon cyan (`#00CFFF / #00E6FF`), dan latar `#080B0E`. Mockup referensi:

![Jacktor Audio UI Landscape](panel-ui/docs/mockup-landscape.png)

- **Statusbar kanan** menampilkan RTC live, mode kipas (AUTO/CUSTOM/FAILSAFE), progres OTA, serta hitung mundur auto-off Bluetooth.
- **Indikator LINK/RX/TX** berada di blok telemetry: LINK solid jika frame diterima <3 detik, RX/TX berkedip 200 ms saat ada paket.
- **Analyzer 16-bar** memvisualisasikan telemetri amplifier secara real-time.
- **Factory Reset** dipindahkan ke halaman Settings dan membutuhkan PIN 4–6 digit (konfirmasi dua kali).
- **Tone Lab** menyediakan preset Simple, Sequence, Musical, dan Randomizer untuk hiburan saat demo.
- **Sinkronisasi data** memakai pola digest/etag (`nv_digest?` → `nv_get`/`nv_set`). NVS menjadi sumber kebenaran; aplikasi hanya menyimpan cache dan otomatis pulih setelah instal ulang.
