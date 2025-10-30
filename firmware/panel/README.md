# Jacktor Panel Bridge Firmware

Firmware ini menjalankan jembatan antara aplikasi panel-ui (desktop/android) dan amplifier Jacktor. Board ESP32 panel menangani negosiasi mode USB-OTG adaptif, fallback tombol daya, serta meneruskan frame JSON antara host dan amplifier secara transparan.

## Build & Flash

1. Pastikan dependensi PlatformIO terpasang.
2. Masuk ke folder firmware panel:
   ```bash
   cd firmware/panel
   pio run
   ```
3. Untuk flash:
   ```bash
   pio run -t upload
   ```
4. Serial monitor default berada pada 921600 baud (`pio device monitor -b 921600`).

Proyek ini memakai tabel partisi bersama `../partitions/jacktor_ota.csv` (dua slot OTA + NVS) yang identik dengan firmware amplifier, sehingga paket rilis berbagi layout memori yang sama.

## Update Firmware Panel

### OTA via USB/Android OTG

- Setelah aplikasi host terhubung dan mengirim `hello`, jalankan perintah CLI berikut di port panel:

  ```text
  panel ota begin size <SIZE> [crc32 <HEX>]
  panel ota write <B64>
  panel ota end [reboot on|off]
  ```

  atau bentuk JSON:

  ```json
  {"type":"panel","cmd":{"ota_begin":{"size":123456,"crc32":"ABCD1234"}}}
  {"type":"panel","cmd":{"ota_write":{"seq":0,"data_b64":"..."}}}
  {"type":"panel","cmd":{"ota_end":{"reboot":true}}}
  ```

- Panel mengeluarkan event `{"type":"panel_ota","evt":"begin_ok|write_ok|end_ok|abort_ok|error"}` sebagai umpan balik. Selama OTA berlangsung, state machine OTG dan bridge UART → amplifier dibekukan untuk mencegah gangguan.

- Perintah `panel ota abort` mengakhiri proses dan memulihkan bridge.

### Flash Langsung

- Tahan tombol **BOOT** pada board panel, tekan **EN/RESET**, lalu lepaskan untuk masuk ke bootloader ESP32 standar.
- Flash menggunakan `pio run -t upload` atau `esptool.py` via port USB panel. Mode ini berguna sebagai jalur pemulihan.

## GPIO Mapping Final

| GPIO | Arah | Fungsi                           | Level aktif / Timing            |
|------|------|----------------------------------|---------------------------------|
| 17   | TX   | UART2 TX → Amplifier             | —                               |
| 16   | RX   | UART2 RX ← Amplifier             | —                               |
| 23   | OUT  | EN Amplifier (reset line)        | LOW aktif                       |
| 27   | OUT  | GPIO0 Amplifier (flash mode)     | LOW aktif                       |
| 32   | OUT  | Trigger tombol Power Android     | LOW 1 s                         |
| 13   | OUT  | USB OTG ID Control               | LOW = ID→GND (minta mode host)  |
| 34   | IN   | VBUS Sense (opto/ADC)            | HIGH = 5V hadir                 |
| 25   | OUT  | LED Merah (status)               | —                               |
| 26   | OUT  | LED Hijau (status)               | —                               |
| 21   | I²C  | SDA (opsional RTC/expander)      | —                               |
| 22   | I²C  | SCL (opsional RTC/expander)      | —                               |
| 0    | IN   | Tombol BOOT Panel                | —                               |
| EN   | IN   | Reset Panel                      | —                               |
| 33   | IN   | Cadangan input-only              | —                               |
| 35   | IN   | Cadangan input                   | —                               |
| 36   | IN   | Cadangan input                   | —                               |
| 39   | IN   | Cadangan input                   | —                               |

## State Machine OTG ID Adaptif

Firmware menjalankan state machine non-blocking berikut (≈10 ms tick) untuk memaksa Android masuk mode host sambil meminimalkan spam toggling:

| State          | LED                 | Aksi utama |
|----------------|--------------------|------------|
| `IDLE`         | LED off             | Evaluasi kondisi awal. Jika VBUS sudah valid langsung ke `WAIT_HANDSHAKE`, bila tidak ke `PROBE`. |
| `PROBE`        | Merah blink cepat   | Tarik `PIN_USB_ID` LOW selama `OTG_PULSE_LOW_MS`, lepaskan, increment counter, evaluasi VBUS. |
| `WAIT_VBUS`    | Merah solid         | Grace `OTG_GRACE_AFTER_VBUS_MS` setelah VBUS stabil. |
| `WAIT_HANDSHAKE` | Kuning (R+G)      | Tunggu frame `hello` dari host hingga `OTG_HANDSHAKE_TIMEOUT_MS`. |
| `HOST_ACTIVE`  | Hijau solid         | Bridge aktif penuh; tidak ada toggling ID. Monitor VBUS loss. |
| `BACKOFF`      | Merah blink pelan   | Jeda sesuai jadwal backoff; boleh memicu fallback power. |
| `COOLDOWN`     | Merah blink pelan   | Pendinginan `OTG_COOLDOWN_MS` sebelum mengulang siklus. |

### Timing & Batasan

| Parameter                     | Nilai                                  |
|-------------------------------|----------------------------------------|
| `OTG_PULSE_LOW_MS`           | 800 ms                                 |
| `OTG_BACKOFF_SCHEDULE_MS`    | 1 s → 2 s → 4 s → 8 s → 15 s → 30 s    |
| `OTG_MAX_PULSES_PER_CYCLE`   | 6 (lebih dari itu → cooldown)          |
| `OTG_MAX_PROBE_DURATION_MS`  | 60 s total percobaan                   |
| `OTG_COOLDOWN_MS`            | 5 menit                                |
| `OTG_VBUS_DEBOUNCE_MS`       | 500 ms                                 |
| `OTG_GRACE_AFTER_VBUS_MS`    | 2 s                                    |
| `OTG_HANDSHAKE_TIMEOUT_MS`   | 10 s                                   |
| `OTG_VBUS_LOSS_MS`           | 2 s (drop lebih lama dianggap putus)   |

Setiap masuk `BACKOFF`, indeks jadwal meningkat hingga batas maksimum. Seluruh transisi dicatat melalui log `[OTG] ...` di port USB (host).

## Fallback Power (GPIO32)

Selain pulse awal saat boot (`POWER_WAKE_ON_BOOT`), firmware memiliki fallback untuk memicu tombol power Android ketika negosiasi OTG gagal:

- Disyaratkan `POWER_WAKE_ON_FAILURE = 1` dan minimal dua pulse OTG telah dicoba (`pulseCount >= 2`).
- Fallback hanya dieksekusi bila belum melewati `POWER_WAKE_MAX_PER_EVENT` (default 1). Setelah digunakan, panel menunggu `POWER_WAKE_COOLDOWN_MS` sebelum boleh memicu lagi.
- Aksi: tarik `PIN_TRIG_PWR` LOW selama `POWER_WAKE_PULSE_MS` (1 s), lalu grace `POWER_WAKE_GRACE_MS` (3 s) sebelum kembali ke `PROBE`.
- Semua power pulse dicatat (`power_pulse reason=fallback`, `power_pulse_done`, `power_grace_done`).

## Handshake JSON Host ↔ Panel

Ketika aplikasi panel-ui membuka port serial, ia wajib mengirim:
```json
{"type":"hello","who":"android|desktop","app_ver":"x.y.z","schema_ver":"1.1"}
```
Panel merespons:
```json
{"type":"ack","ok":true,"msg":"hello_ack","host":"ok"}
```
Setelah `hello_ack`, state berpindah ke `HOST_ACTIVE`, LED hijau solid, dan toggling ID dihentikan. Jika handshake tidak diterima dalam `OTG_HANDSHAKE_TIMEOUT_MS`, panel kembali melakukan `PROBE`/`BACKOFF`.

## Bridge UART Panel ↔ Amplifier

- Port USB (Serial) ↔ aplikasi host.
- UART2 (Serial2) ↔ amplifier, 921600 baud.
- Frame berbasis newline (`\n`), JSON diteruskan apa adanya dua arah.
- Baris kosong diabaikan; frame yang melebihi `BRIDGE_MAX_FRAME` (512 byte) ditolak dan dilog.
- Logging panel (`[OTG] ...`) ikut tampil di port USB agar UI dapat men-debug state mesin.

### Routing Perintah & CLI

- Perintah **tanpa prefix** diproses sebagai kontrol amplifier dan diterjemahkan ke JSON sebelum diteruskan (mis. `ota begin size ...`).
- Perintah yang diawali `panel` ditangani lokal (`panel ota ...`, `panel otg status`, dsb.).
- Baris yang langsung diawali `{` akan diparsing sebagai JSON; `{"type":"panel",...}` ditangani lokal sementara `{"type":"cmd",...}` diteruskan ke amplifier.
- Panel mengirim ACK standar `{"type":"ack","ok":true|false,"cmd":"..."}` untuk setiap perintah CLI agar host mudah melakukan error handling.

## LED Status

| Pola LED | Makna |
|----------|-------|
| Merah solid singkat saat boot | Inisialisasi firmware.
| Merah blink cepat             | State `PROBE` (toggling ID aktif).
| Merah blink pelan             | State `BACKOFF` atau `COOLDOWN`.
| Kuning (Merah + Hijau)        | `WAIT_HANDSHAKE` (VBUS valid, menunggu `hello`).
| Hijau solid                   | `HOST_ACTIVE` (bridge siap, ID dilepas).
| Semua mati                    | `IDLE` tanpa host.

## Skenario Uji

1. **Android sleep (VBUS tetap ON)** — Panel langsung mendeteksi VBUS valid, masuk `WAIT_HANDSHAKE`, menerima `hello`, lalu `HOST_ACTIVE` tanpa fallback power.
2. **Android benar-benar mati (VBUS LOW)** — Panel melakukan beberapa `PROBE` + jadwal backoff, memicu fallback power sekali, menunggu grace, kemudian berhasil memperoleh VBUS dan `hello` → `HOST_ACTIVE`.
3. **Kabel USB dicabut** — Saat `HOST_ACTIVE`, VBUS drop lebih dari 2 s → log `host_active_vbus_lost`, siklus reset ke `PROBE` untuk mencari host baru.

Dokumentasi ini mencerminkan perilaku firmware yang tersimpan di `src/main.cpp` dan dapat dijadikan referensi saat menyelaraskan panel-ui desktop maupun Android.
