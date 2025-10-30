# Jacktor Amplifier (ESP32 Classic)

Firmware amplifier berbasis **ESP32** untuk unit "Jacktor". Perangkat ini mengelola proteksi SMPS, kipas, input Bluetooth/AUX,
UI OLED, dan telemetri JSON ke panel Android. Seluruh parameter runtime dipersist di **NVS** (kecuali relay utama yang wajib OFF
saat boot dingin) sehingga konfigurasi bertahan lintas restart.

---

## Daftar Isi

1. [Fitur Utama](#fitur-utama)
2. [Factory Reset](#factory-reset)
3. [GPIO Mapping](#gpio-mapping)
4. [Build & Flash](#build--flash)
5. [Telemetri JSON](#telemetri-json)
6. [Skema Command UART](#skema-command-uart)
7. [Catatan OTA](#catatan-ota)
8. [Kebijakan RTC Sync](#kebijakan-rtc-sync)
9. [Limitasi](#limitasi)

---

## Fitur Utama

| Kelompok | Ringkasan |
|----------|-----------|
| **Proteksi & Power** | Proteksi SMPS 65 V dengan ambang cut/recovery yang dapat dikonfigurasi (opsi bypass sementara). Relay utama selalu OFF saat boot; auto-power mengikuti sinyal PC detect (GPIO34). |
| **Monitoring** | Voltmeter ADS1115 (divider R1=201.2 kΩ / R2=9.65 kΩ) dan sensor suhu heatsink DS18B20. |
| **Audio & Analitik** | FFT analyzer 16 band dan VU meter 0..1023, aktif hanya saat amplifier ON untuk efisiensi. |
| **Pendinginan** | Mode kipas AUTO/CUSTOM/FAILSAFE dengan PWM 25 kHz dan self-test singkat di awal boot. |
| **Antarmuka** | OLED 128×64: splash screen, jam besar saat standby, layar RUN dengan status input, tegangan, suhu, VU/analyzer. |
| **Telemetri** | Telemetri JSON stabil (10 Hz ketika aktif, 1 Hz sinkron SQW DS3231 saat standby) berikut status OTA dan konfigurasi NVS. |
| **OTA & RTC** | OTA streaming via UART (CRC32 + ack per chunk) dan sinkronisasi RTC dengan kebijakan offset > 2 s serta rate-limit 24 jam. |
| **Persistensi** | Semua pengaturan runtime disimpan di NVS; factory reset tersedia via kombinasi tombol Power+BOOT maupun perintah UART. |

---

## Factory Reset

Kedua jalur berikut menghapus seluruh NVS, menampilkan pesan pada OLED, membunyikan buzzer dua kali, memastikan relay utama tetap OFF, menerbitkan log `factory_reset_executed`, dan melakukan reboot otomatis.

### A. Manual Combo (Power + BOOT)

1. Saat menyalakan amplifier, tekan dan tahan tombol Power (GPIO25) bersama tombol BOOT (GPIO0).
2. Setelah ±1 detik, OLED menampilkan `FACTORY RESET`, buzzer berbunyi dua kali, dan perangkat langsung menghapus NVS sebelum restart.

### B. Factory Reset via Panel (UART)

1. Pastikan amplifier berada dalam kondisi standby (`powerIsOn() == false`).
2. Kirim perintah berikut:

   ```json
   {"type":"cmd","cmd":{"factory_reset":true}}
   ```

3. Panel menerima ACK sukses:

   ```json
   {"type":"ack","ok":true,"changed":"factory_reset","value":true}
   ```

4. Amplifier menampilkan `FACTORY RESET (UART)` dan melakukan reboot setelah NVS dibersihkan.
5. Jika perintah dikirim saat amplifier ON, balasan menjadi `{"type":"ack","ok":false,"changed":"factory_reset","error":"system_active"}` dan reset tidak dijalankan.

---

## GPIO Mapping

| GPIO | Fungsi | Catatan |
|-----:|--------|---------|
| 2 | UART activity LED | Indikasi TX/RX UART2 |
| 14 | Relay utama | OFF default saat boot |
| 16 / 17 | UART2 RX/TX | Ke panel / Android |
| 25 | Tombol Power / Speaker power switch | Digunakan juga untuk combo factory reset |
| 26 | Speaker selector | Persist di NVS |
| 4 | Bluetooth enable | Auto-off 5 menit idle/AUX |
| 23 | Status Bluetooth (aktif LOW) | AUX→LOW≥3 s→BT |
| 5 / 19 / 18 | Tombol Play / Prev / Next | Kontrol modul Bluetooth |
| 27 | DS18B20 sensor suhu | Heatsink |
| 32 | Fan PWM output | Mode AUTO/CUSTOM/FAILSAFE |
| 33 | Buzzer | Klik / error |
| 35 | RTC SQW input | 1 Hz DS3231 |
| 36 | I²S analyzer input | FFT 16 band |
| 34 | PC detect via opto | LOW = PC ON |
| 21 / 22 | I²C SDA/SCL | RTC + ADS1115 + OLED |
| 39 | Speaker protector sense | HIGH = normal |

---

## Build & Flash

1. **Install PlatformIO CLI** – ikuti panduan resmi: <https://docs.platformio.org/en/latest/core/index.html>.
2. **Dependensi otomatis** – PlatformIO akan menarik ArduinoJson v7, Adafruit ADS1X15, OneWire, DallasTemperature, RTClib, U8g2, dan arduinoFFT.
3. **Build firmware** – jalankan `pio run`.
4. **Upload via UART** – gunakan `pio run -t upload`.
5. **Monitor serial** – `pio device monitor -b 921600` (opsional).

Firmware memakai skema partisi OTA ganda `partitions/jacktor_ota.csv`; slot OTA cadangan diisi melalui perintah UART `ota_*`.

---

## Telemetri JSON

- Mode ON: 10 Hz.
- Mode Standby: 1 Hz, dipicu tepat di tepi SQW 1 Hz DS3231 (fallback internal timer bila SQW hilang).
- Contoh payload minimal:

```json
{
  "type": "telemetry",
  "ver": "1",
  "data": {
    "time": "2025-10-30T12:34:56Z",
    "fw_ver": "amp-1.0.0",
    "ota_ready": true,
    "smps_v": 53.8,
    "heat_c": 36.2,
    "inputs": {"bt": true, "speaker": "big"},
    "states": {"on": true, "standby": false},
    "errors": ["LOW_VOLTAGE"],
    "an": [4,6,9,12,15,18,13,9,6,4,3,2,1,1,0,0],
    "vu": 712,
    "nvs": {
      "fan_mode": 0,
      "fan_mode_str": "auto",
      "fan_duty": 640,
      "spk_big": true,
      "spk_pwr": true,
      "bt_en": true,
      "bt_autooff": 300000,
      "smps_bypass": false,
      "smps_cut": 50.0,
      "smps_rec": 52.0
    }
  }
}
```

`errors` berisi kombinasi `LOW_VOLTAGE`, `NO_POWER`, `SENSOR_FAIL`, dan/atau `SPK_PROTECT` (boleh kosong).

---

## Skema Command UART

Semua request memakai struktur `{"type":"cmd","cmd":{...}}`. Balasan standar untuk setter konfigurasi:

```json
{"type":"ack","ok":true,"changed":"<key>","value":<val>}
```

Apabila gagal: `{"type":"ack","ok":false,"changed":"<key>","error":"range|invalid|nvs_fail"}`.

### Kontrol dasar

| Command | Keterangan |
|---------|------------|
| `{"type":"cmd","cmd":{"power":true}}` | ON/OFF relay utama |
| `{"type":"cmd","cmd":{"bt":false}}` | Enable / disable modul Bluetooth |
| `{"type":"cmd","cmd":{"spk_sel":"small"}}` | Pilih speaker kecil |
| `{"type":"cmd","cmd":{"spk_pwr":true}}` | Suplai speaker protector |
| `{"type":"cmd","cmd":{"buzz":{"ms":60,"d":500}}}` | Pola buzzer kustom |
| `{"type":"cmd","cmd":{"nvs_reset":true}}` | Reset konfigurasi NVS |
| `{"type":"cmd","cmd":{"factory_reset":true}}` | Factory reset lengkap (hanya standby) |

### Konfigurasi NVS

| Command | Tipe & Rentang |
|---------|----------------|
| `smps_bypass` | `true|false` |
| `smps_cut` | `float` 30.0–70.0 V (harus < `smps_rec`) |
| `smps_rec` | `float` 30.0–80.0 V (harus > `smps_cut`) |
| `bt_autooff` | `uint32` milidetik, 0–3.600.000 |
| `fan_mode` | `"auto"|"custom"|"failsafe"` |
| `fan_duty` | `int` 0–1023 (aktif bila mode custom) |

### RTC Sync

- `{"type":"cmd","cmd":{"rtc_set":"YYYY-MM-DDTHH:MM:SS"}}`
- `{"type":"cmd","cmd":{"rtc_set_epoch":1698653727}}`

Jika diterapkan, amplifier mengirim:

```json
{"type":"log","lvl":"info","msg":"rtc_synced","offset_sec":12}
```

Jika ditolak karena offset kecil atau rate-limit 24 jam:

```json
{"type":"log","lvl":"warn","msg":"rtc_sync_skipped","reason":"offset_small|ratelimited"}
```

### OTA Streaming

1. **Begin**
   ```json
   {"type":"cmd","cmd":{"ota_begin":{"size":123456,"crc32":"ABCD1234"}}}
   ```
   Respon: `{"type":"ota","evt":"begin_ok"}` atau `{"type":"ota","evt":"begin_err","err":"..."}`

2. **Write**
   ```json
   {"type":"cmd","cmd":{"ota_write":{"seq":1,"data_b64":"AAEC..."}}}
   ```
   Respon: `{"type":"ota","evt":"write_ok","seq":1}` atau `{"type":"ota","evt":"write_err","seq":1,"err":"..."}`

3. **End**
   ```json
   {"type":"cmd","cmd":{"ota_end":{"reboot":true}}}
   ```
   Respon: `{"type":"ota","evt":"end_ok","rebooting":true}` atau `{"type":"ota","evt":"end_err","err":"..."}`

4. **Abort**
   ```json
   {"type":"cmd","cmd":{"ota_abort":true}}
   ```
   Respon: `{"type":"ota","evt":"abort_ok"}`

Semua error OTA juga disiarkan sebagai `{"type":"ota","evt":"error","err":"..."}`. Ketika OTA aktif, auto-power dari PC detect diabaikan.

---

## Catatan OTA

- Alur standar: `ota_begin` → beberapa `ota_write` berurutan → `ota_end` (pilih `reboot:true` untuk restart otomatis atau `false` untuk menunggu perintah manual).
- Selama OTA berjalan, guard internal memaksa `ota_ready=false` pada telemetri dan menonaktifkan auto-power PC detect.
- Jika `ota_end` diminta dengan `reboot:true`, status guard tetap aktif sampai restart selesai agar panel tidak memicu ulang secara prematur.
- `ota_abort` kapan saja mengembalikan amplifier ke mode normal dan mengatur `ota_ready=true`.

---

## Kebijakan RTC Sync

- Sinkronisasi hanya dijalankan jika |offset| > 2 detik dibanding RTC lokal.
- Setelah berhasil, timestamp sync disimpan di NVS (`stateSetLastRtcSync`).
- Permintaan berikutnya baru diproses setelah 24 jam sejak sync terakhir.
- OSF (oscillator stop flag) DS3231 dibersihkan otomatis setelah RTC disetel.
