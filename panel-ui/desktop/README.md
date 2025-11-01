# Jacktor Panel Desktop (Electron + React)

## Build & Run

```bash
cd panel-ui/desktop
npm install
npm run dev        # Vite dev server + Electron shell
npm run build      # Bundle React assets (dist/)
npm start          # Launch Electron menggunakan build
```

## USB & Port Selection
- Aplikasi otomatis mencari perangkat serial dengan nama produk/manufaktur mengandung "panel".
- Jika panel belum terdeteksi, hubungkan via USB dan jalankan ulang aplikasi atau klik "Refresh Cache" di Settings.
- Console CLI selalu meneruskan perintah mentah ke panel (`window.panelSerial.sendCli`).

## CLI Ringkas
- `help`, `show status`, `show config`
- `power on/off`, `set speaker-selector big|small`
- `bt enable|disable|play|pause|next|prev`
- `fan mode auto|custom|failsafe`, `fan duty <0-100>`
- `smps protect toggle|status`
- `rtc sync`, `reset nvs --force --pin <1234>`
- `tone {"type":"simple"}` (Tone Lab presets)

## Catatan
- Cache NV (`nv-cache.json`) disimpan di direktori `userData` Electron dan dikelola via IPC (`nvCache:*`).
- OTA manager akan mengirim `ota start <target>` lalu `ota commit` sesuai pilihan pengguna.
- Semua tampilan dioptimalkan untuk landscape dengan tema neon cyan agar konsisten dengan panel fisik.
