# Jacktor Panel-UI Android (WIP)

Target aplikasi Android (Kotlin + Jetpack Compose) dengan spesifikasi utama:

- `compileSdk = 36` / `targetSdk = 36` (Android 16) dan `minSdk = 26`.
- Integrasi USB Host memakai `com.github.mik3y:usb-serial-for-android` untuk komunikasi dengan panel bridge.
- Foreground service yang menjaga koneksi serial, menyiarkan state melalui `StateFlow`, dan menampilkan notifikasi status.
- Tampilan Home berisi statusbar, analyzer 16 band, kontrol sinkron, dan konsol CLI → JSON.
- Mode hemat daya yang menurunkan refresh analyzer saat aplikasi ke background.

Blueprint modul gradle dan implementasi Compose akan ditambahkan setelah kerangka layanan USB rampung.
