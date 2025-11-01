# Jacktor Panel Android (Jetpack Compose)

## Build & Install

```bash
cd panel-ui/android
./gradlew assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
```

- Target SDK 35, min SDK 26 (USB OTG support).
- Jalankan `./gradlew installDebug` untuk push langsung ke perangkat terhubung.

## Hak Akses USB
- Saat pertama kali menyambungkan panel, aplikasi akan meminta izin USB-serial (library `usb-serial-for-android`).
- Aktifkan opsi "Always use" agar koneksi bertahan saat layar mati.
- Opsional: jalankan foreground service agar koneksi tidak drop saat aplikasi di background.

## Navigasi
- Home menampilkan Analyzer 16-bar, Controls (Power/BT/Speaker/Fan/SMPS), Status Cards, Link indicators, dan StatusBar.
- Settings memuat Factory Reset (PIN 4–6 digit + konfirmasi), RTC Sync Now, Tone Lab presets, Diagnostics (Open Console).
- Console menyediakan log real-time + input CLI, tombol Save log akan mengekspor file teks di penyimpanan aplikasi.

## Sinkronisasi Data
- Repository menggunakan DataStore Preferences untuk preferensi ringan dan file JSON (`nv-cache.json`) untuk dump NV besar.
- Alur digest/etag identik dengan desktop: `nv_digest?` → bandingkan etag → `nv_get` bila berubah → `nv_set` memakai `If-Match`.
- Cache otomatis dipulihkan saat aplikasi dipasang ulang.

## CLI Ringkas
- `help`, `show status`, `show config`
- `power on/off`
- `bt enable|disable|play|pause|next|prev`
- `set speaker-selector big|small`
- `fan mode auto|custom|failsafe`, `fan duty <0-100>`
- `smps protect toggle|status`
- `rtc sync`, `reset nvs --force --pin <1234>`
- `tone {"type":"simple"}` dan variasinya
