# Jacktor Audio Amplifier (ESP32)

Firmware **Jacktor Audio Amplifier** berbasis ESP32. Perangkat ini mengelola proteksi SMPS, kipas, input Bluetooth/AUX, UI OLED,
telemetri JSON ke panel bridge, serta pola buzzer non-blocking. Seluruh parameter runtime dipersist di **NVS** (kecuali relay
utama yang wajib OFF saat boot dingin) sehingga konfigurasi bertahan lintas restart.

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
| **Proteksi & Power** | Proteksi SMPS 65 V dengan ambang cut/recovery yang dapat dikonfigurasi (opsi bypass sementara) serta sakelar fitur `FEAT_SMPS_PROTECT_ENABLE` dan `SAFE_MODE_SOFT` untuk diagnostik cepat. Relay utama selalu OFF saat boot; auto-power mengikuti sinyal PC detect (GPIO34) jika `FEAT_PC_DETECT_ENABLE=1`. |
| **Monitoring** | Voltmeter ADS1115 (divider R1=201.2 kΩ / R2=9.65 kΩ), sensor suhu heatsink DS18B20, serta pembacaan suhu internal RTC DS3231 (`rtc_c`). |
| **Audio & Analitik** | FFT analyzer 16 band dan VU meter 0..1023, aktif hanya saat amplifier ON untuk efisiensi. |
| **Pendinginan** | Mode kipas AUTO/CUSTOM/FAILSAFE dengan PWM 25 kHz, self-test (`FEAT_FAN_BOOT_TEST`) dan duty khusus yang disimpan di NVS. |
| **Antarmuka** | OLED 128×64: splash screen, jam besar saat standby, layar RUN dengan status input, tegangan, suhu (termasuk indikator SPEAKER_PROTECT_FAIL), VU/analyzer, serta pola buzzer non-blocking. |
| **Telemetri** | Telemetri JSON stabil (10 Hz ketika aktif, 1 Hz sinkron SQW DS3231 saat standby) berisi blok `features{}` yang mencerminkan flag `FEAT_*`, status OTA, `rtc_c`, daftar error termasuk `SPEAKER_PROTECT_FAIL`, dan snapshot NVS. |
| **OTA & RTC** | OTA streaming via UART (CRC32 + ack per chunk) dan sinkronisasi RTC dengan kebijakan offset > 2 s serta rate-limit 24 jam (`FEAT_RTC_SYNC_POLICY`). |
| **Persistensi** | Semua pengaturan runtime disimpan di NVS; factory reset tersedia via kombinasi tombol Power+BOOT maupun perintah UART. |

---

## Factory Reset

Kedua jalur berikut menghapus seluruh NVS, menampilkan pesan pada OLED, membunyikan buzzer dua kali, memastikan relay utama tetap OFF, menerbitkan log `factory_reset_executed`, dan melakukan reboot otomatis.

### A. Manual Combo (Power + BOOT)

1. Saat menyalakan amplifier, tekan dan tahan tombol Power utama (`BTN_POWER_PIN`, GPIO13) bersama tombol BOOT (GPIO0).
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
| 4 | Bluetooth enable | Auto-off 5 menit idle/AUX; default mengikuti `FEAT_BT_ENABLE_AT_BOOT` |
| 5 / 19 / 18 | Tombol Play / Prev / Next | Kontrol modul Bluetooth |
| 13 | Tombol Power (`BTN_POWER_PIN`) | Aktif LOW, debounced; dipakai combo factory reset |
| 14 | Relay utama | OFF default saat boot |
| 16 / 17 | UART2 RX/TX | Ke panel bridge |
| 21 / 22 | I²C SDA/SCL | RTC + ADS1115 + OLED |
| 23 | Status Bluetooth (aktif LOW) | AUX→LOW≥3 s→BT (`FEAT_BT_AUTOSWITCH_AUX`) |
| 25 | Speaker power switch | Suplai modul proteksi speaker |
| 26 | Speaker selector | Persist di NVS |
| 27 | DS18B20 sensor suhu | Heatsink |
| 32 | Fan PWM output | Mode AUTO/CUSTOM/FAILSAFE; self-test via `FEAT_FAN_BOOT_TEST` |
| 33 | Buzzer | Pola non-blocking LEDC |
| 34 | PC detect via opto | LOW = PC ON (`FEAT_PC_DETECT_ENABLE`) |
| 35 | RTC SQW input | 1 Hz DS3231 |
| 36 | I²S analyzer input | FFT 16 band |
| 39 | Speaker protector sense | HIGH = normal; fault → `SPEAKER_PROTECT_FAIL` |

---

## Build & Flash

1. **Install PlatformIO CLI** – ikuti panduan resmi: <https://docs.platformio.org/en/latest/core/index.html>.
2. **Dependensi otomatis** – PlatformIO akan menarik ArduinoJson v7, Adafruit ADS1X15, OneWire, DallasTemperature, RTClib, U8g2, dan arduinoFFT.
3. **Masuk ke direktori** – `cd firmware/amplifier`.
4. **Build firmware** – jalankan `pio run`.
5. **Upload via UART** – gunakan `pio run -t upload`.
6. **Monitor serial** – `pio device monitor -b 921600` (opsional).

Firmware memakai skema partisi OTA ganda `../partitions/jacktor_audio_ota.csv`; berkas partisi ini **dibagi** dengan firmware Jacktor Audio Panel Bridge sehingga ukuran maksimum image antar-perangkat seragam.

### Update Firmware

1. **OTA via Panel (disarankan)**
   - Pastikan panel bridge telah tersambung ke amplifier dan aplikasi host (desktop/Android) sudah melakukan handshake.
   - Kirim urutan perintah berikut melalui panel:
     ```text
     ota begin size <SIZE> [crc32 <HEX>]
     ota write <B64>
     ota end [reboot on|off]
     ```
     atau gunakan format JSON langsung:
     ```json
     {"type":"cmd","cmd":{"ota_begin":{"size":123456,"crc32":"ABCD1234"}}}
     {"type":"cmd","cmd":{"ota_write":{"seq":0,"data_b64":"..."}}}
     {"type":"cmd","cmd":{"ota_end":{"reboot":true}}}
     ```
   - Amplifier menerbitkan event `{"type":"ota","evt":"begin_ok|write_ok|end_ok|abort_ok|error"}` untuk tiap tahap. Selama OTA aktif, panel menahan aksi destruktif lain.
   - Field telemetri `features{}` mencerminkan flag `FEAT_*` yang sedang aktif sehingga UI dapat menyesuaikan perilaku.

2. **Flash langsung via USB amplifier**
   - Buka cover amplifier, sambungkan port micro-USB bawaan ke PC.
   - Masuk ke `firmware/amplifier`, kemudian jalankan `pio run -t upload` atau gunakan `esptool.py` seperti biasa.
   - Jalur RX0/TX0 melalui panel **tidak tersedia**; update langsung hanya lewat port USB internal amplifier.

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
    "rtc_c": 28.5,
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
      "bt_en": false,
      "bt_autooff": 300000,
      "smps_bypass": false,
      "smps_cut": 50.0,
      "smps_rec": 52.0
    },
    "features": {
      "pc_detect": false,
      "bt_boot_on": false,
      "bt_autoswitch": true,
      "fan_boot_test": true,
      "factory_reset_combo": true,
      "rtc_temp": true,
      "rtc_sync_policy": true,
      "smps_protect": true,
      "ds18b20_softfilter": false,
      "safe_mode": false
    }
  }
}
```

`errors` berisi kombinasi `LOW_VOLTAGE`, `NO_POWER`, `SENSOR_FAIL`, dan/atau `SPEAKER_PROTECT_FAIL` (boleh kosong).

---

## Feature Toggles & Buzzer

Semua sakelar diagnostik tersedia di `include/config.h` sehingga perilaku firmware dapat diubah tanpa menyentuh modul lain.

- `FEAT_PC_DETECT_ENABLE` — aktifkan otomatis ON/OFF berbasis sinyal PC detect.
- `FEAT_BT_ENABLE_AT_BOOT` — tentukan apakah modul BT menyala otomatis saat boot.
- `FEAT_BT_AUTOSWITCH_AUX` — izinkan pindah AUX↔BT ketika level AUX menahan LOW ≥3 s.
- `FEAT_FAN_BOOT_TEST` — jalankan self-test kipas beberapa ratus milidetik saat boot.
- `FEAT_FACTORY_RESET_COMBO` — kombinasikan BTN_POWER + BOOT di startup untuk factory reset.
- `FEAT_RTC_TEMP_TELEMETRY` — sertakan `rtc_c` di telemetri.
- `FEAT_RTC_SYNC_POLICY` — tegakkan syarat offset >2 s dan rate-limit 24 jam saat sync RTC.
- `FEAT_SMPS_PROTECT_ENABLE` — hidup/matikan logika proteksi tegangan SMPS.
- `FEAT_FILTER_DS18B20_SOFT` — aktifkan filter software suhu DS18B20 (opsional).
- `SAFE_MODE_SOFT` — paksa output kritis OFF (relay, speaker power, BT) untuk troubleshooting.

Buzzer LEDC (GPIO33) berjalan non-blocking. Pola default:

| Event | Pola |
|-------|------|
| Boot selesai init | Nada menaik 880→1175→1568 Hz |
| Masuk standby / shutdown | Nada menurun 1568→1175→880 Hz |
| Pindah ke BT | 1568 Hz 60 ms, jeda 40 ms, 2093 Hz 80 ms |
| Pindah ke AUX | 1175 Hz 60 ms |
| ACK command sukses | Klik 3 kHz 25 ms |
| Warning berulang | 1175 Hz 60 ms setiap 2.5 s |
| Error berulang (termasuk `SPEAKER_PROTECT_FAIL`) | 880 Hz 70 ms dua kali setiap 1.2 s |

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
