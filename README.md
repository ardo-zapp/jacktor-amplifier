# Jacktor Monorepo

Jacktor adalah proyek amplifier pintar dengan panel kontrol dan antarmuka pengguna lintas platform. Repositori ini sedang direstrukturisasi menjadi monorepo yang menampung firmware perangkat keras dan aplikasi panel-ui.

## Struktur Repository

```
firmware/
  amplifier/   # Firmware ESP32 untuk unit amplifier utama
  panel/       # (WIP) Firmware jembatan panel ESP32
panel-ui/
  desktop/     # (WIP) Aplikasi desktop (Electron + React)
  android/     # (WIP) Aplikasi Android (Compose, target Android 16)
```

Lihat README di masing-masing sub-proyek untuk detail lebih lanjut.

## Status Saat Ini

- Firmware amplifier telah dipindahkan ke `firmware/amplifier/` dan tetap build menggunakan PlatformIO.
- Direktori lain masih disiapkan sebagai kerangka kerja untuk penambahan firmware panel serta aplikasi panel-ui.

## Langkah Berikutnya

- Melengkapi firmware panel dengan mapping GPIO final dan dokumentasi operasional.
- Mengembangkan panel-ui (desktop & Android) termasuk analyzer audio, konsol CLI JSON, dan kontrol sinkron.
- Menambahkan pipeline CI/CD untuk build firmware dan aplikasi.

Kontribusi dan diskusi arsitektur dipersilakan melalui issue tracker.
