# Jacktor Monorepo

Jacktor adalah ekosistem amplifier pintar berbasis ESP32 lengkap dengan panel bridge, aplikasi desktop (Electron), serta aplikasi Android (Compose) untuk kontrol dan telemetri. Repositori ini menampung seluruh komponen tersebut dalam satu monorepo.

## Struktur Repository

```
firmware/
  amplifier/    # Firmware ESP32 untuk unit amplifier utama
  panel/        # Firmware ESP32 panel bridge (USB OTG + UART bridge)
  partitions/   # Tabel partisi OTA bersama (jacktor_ota.csv)
panel-ui/
  desktop/      # (WIP) Aplikasi desktop Electron + React
  android/      # (WIP) Aplikasi Android (Compose, target Android 16)
```

Masing-masing subdirektori memiliki README yang mendokumentasikan GPIO, fitur, dan instruksi build spesifik.

## Jalur Update Firmware

| Perangkat   | Mode OTA                                                                 | Mode Flash Langsung                           |
|-------------|---------------------------------------------------------------------------|-----------------------------------------------|
| Amplifier   | Via panel bridge (UART2) menggunakan perintah `ota_*` line-based JSON.   | Colok micro-USB amplifier langsung dan flash memakai esptool/PlatformIO. |
| Panel Bridge| Via USB CDC/Android OTG memakai perintah `panel ota *` (CLI/JSON).       | Tekan BOOT + EN pada board panel dan flash melalui port USB panel.        |

- Kedua firmware memakai **file partisi yang sama**: `firmware/partitions/jacktor_ota.csv` (layout dual-slot OTA + NVS). Hal ini menyederhanakan distribusi dan rollback karena image amplifier maupun panel memiliki ukuran maksimum yang sama.
- Selama OTA panel berlangsung, mesin OTG serta bridge UART dipause sehingga update tidak terganggu.
- OTA amplifier dijembatani panel tanpa kebutuhan jalur RX0/TX0 fisik; semua update rilis dilakukan via panel atau USB langsung pada perangkat amplifier.

## Dokumentasi Lanjutan

- `firmware/amplifier/README.md` — Fitur lengkap amplifier, skema telemetri (termasuk `rtc_c`), kebijakan OTA & RTC, serta langkah update.
- `firmware/panel/README.md` — Mapping GPIO final, state machine OTG adaptif, fallback power, handshake, dan instruksi OTA panel.
- `panel-ui/desktop/README.md` & `panel-ui/android/README.md` — Roadmap UI, dependensi build, dan spesifikasi CLI→JSON.

Kontribusi dipersilakan. Ajukan issue atau pull request untuk diskusi lebih lanjut.
