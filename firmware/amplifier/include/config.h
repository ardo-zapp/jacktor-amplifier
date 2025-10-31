#pragma once
/*
  Jacktor Audio Amplifier — config.h
  --------------------------------
  Pusat pengaturan firmware. Ubah hanya di file ini kalau mau:
   - Ganti pin mapping
   - Kalibrasi divider tegangan (R1/R2)
   - Kurva kipas & mode default
   - Perilaku BT auto-off
   - Kecepatan telemetri
   - Parameter Analyzer (FFT/I2S)
   - OTA limit, dll.

  Catatan penting:
  - Semua modul include config.h paling awal agar makro konsisten.
  - RELAY utama SELALU default OFF saat boot dingin; tidak disimpan NVS.
  - Nilai resistor R1/R2 pakai nilai REAL hasil ukur (201.2kΩ & 9.65kΩ).
*/


// ============================================================================
//  DS18B20 (heatsink sensor)
#define DS18B20_PIN            27

//  Firmware meta
// ============================================================================
#define FW_NAME                  "Jacktor Audio Amplifier Firmware"
#define FW_VERSION               "amp-1.0.0"


// ============================================================================
//  Diagnostic feature toggles & global policy
// ============================================================================
#define FEAT_PC_DETECT_ENABLE        0   // 0=matikan auto-power via PC detect
#define FEAT_BT_ENABLE_AT_BOOT       0   // 0=BT default OFF saat boot
#define FEAT_BT_AUTOSWITCH_AUX       1   // 1=atur AUX↔BT (LOW≥3s dst)
#define FEAT_FAN_BOOT_TEST           1
#define FEAT_FACTORY_RESET_COMBO     1
#define FEAT_RTC_TEMP_TELEMETRY      1   // kirim rtc_c di telemetri
#define FEAT_RTC_SYNC_POLICY         1   // offset>2s + rate-limit 24h
#define FEAT_SMPS_PROTECT_ENABLE     1
#define FEAT_FILTER_DS18B20_SOFT     0   // filter software opsional

#define PC_DETECT_ACTIVE_LOW         1
#define PC_DETECT_INPUT_PULL         INPUT_PULLUP
#define BTN_POWER_PIN                13      // tombol power utama (input), aktif LOW
#define BTN_BOOT_PIN                 0
#define BTN_POWER_ACTIVE_LOW         1
#define BT_STATUS_ACTIVE_LOW         1

#define UI_BOOT_HOLD_MS              900
#define PC_DETECT_GRACE_MS           2500
#define PC_DETECT_DEBOUNCE_MS        80
#define AUX_TO_BT_LOW_MS             3000
#define FAN_BOOT_TEST_MS             800
#define BT_AUTO_OFF_IDLE_MS          300000

#define SMPS_CUT_V                   50.0f
#define SMPS_REC_V                   52.0f

#define SAFE_MODE_SOFT               0

// ============================================================================
//  Serial / UART
// ============================================================================
#define SERIAL_BAUD_USB          115200      // USB-CDC monitor
#define SERIAL_BAUD_LINK         115200      // UART2 ke Panel (telemetri & command)

// Logging UART internal (Serial) -- default aktif
#ifndef LOG_ENABLE
#define LOG_ENABLE               1
#endif
#ifndef LOG_BAUD
#define LOG_BAUD                 SERIAL_BAUD_USB
#endif

// UART2 (ke Panel)
#define UART2_RX_PIN             16
#define UART2_TX_PIN             17

// LED aktivitas UART (indikasi TX/RX). Tetap di GPIO2 (hanya LED).
#define LED_UART_PIN             2


// ============================================================================
//  Tombol fisik
// ============================================================================
#define BTN_POWER_INPUT_MODE     INPUT_PULLUP
#define BTN_FACTORY_RESET_HOLD_MS 1000


// ============================================================================
//  I²C backbone (RTC + ADS1115 + OLED)
// ============================================================================
#define I2C_SDA                  21
#define I2C_SCL                  22


// ============================================================================
//  RTC DS3231
//  - Panel yang mengatur sync waktu → Amplifier (rate-limit & hanya bila offset besar)
//  - SQW 1 Hz dipakai untuk pacing telemetri saat STANDBY
// ============================================================================
#define RTC_I2C_ADDR             0x68
#define RTC_SQW_PIN              35     // Input SQW 1 Hz
#define RTC_SYNC_MIN_OFFS_SEC    2      // Tulis RTC hanya jika offset > 2s
#define RTC_SYNC_MIN_INTERVAL_H  24     // Rate limit sync (jam): 24h


// ============================================================================
//  OLED (opsional; standby hanya jam. VU meter & info lain saat ON di layar amp.)
//  - Jika hardware OLED tidak terpasang, fungsi UI akan no-op.
// ============================================================================
#define OLED_I2C_ADDR            0x3C
#define OLED_W                   128
#define OLED_H                   64
#define ULTICK_MS                10     // Interval uiTick() untuk refresh ringan


// ============================================================================
//  Voltmeter: ADS1115 (EKSTERNAL) — TANPA smoothing
//  - Divider R1 ke Vin (atas) dan R2 ke GND (bawah)
//  - Gunakan nilai REAL hasil pengukuran agar akurat
// ============================================================================
#define ADS_I2C_ADDR             0x48
#define ADS_CHANNEL              0       // 0..3 (single-ended)
#define R1_OHMS                  201200.0f  // 201.2 kΩ
#define R2_OHMS                  9650.0f    // 9.65 kΩ

// Bawah ambang ini dianggap “tidak ada daya” (noise floor)
// (0.10 V dipilih agar tidak terkunci 0.0V namun tetap menolak noise)
#define VOLT_MIN_VALID_V         0.10f


// ============================================================================
//  Proteksi SMPS 65V
//  - cutoff  : jika ON dan V < cutoff → trip ke STANDBY
//  - recovery: ambang untuk “aman ON” lagi
//  - bypass  : abaikan proteksi (untuk testing)
// ============================================================================
#define SMPS_CUTOFF_V            SMPS_CUT_V
#define SMPS_RECOVERY_V          SMPS_REC_V
#define SMPS_PROTECT_BYPASS      (!FEAT_SMPS_PROTECT_ENABLE)


// ============================================================================
//  Bluetooth module
//  - BT_ENABLE_PIN: power modul BT
//  - BT_STATUS_PIN: dari LED modul, AKTIF LOW saat BT mode (real-time)
//  - Auto-off: kalau tetap AUX (tidak masuk BT) > 5 menit → matikan modul
//  - AUX → BT butuh LOW stabil ≥ 3 detik; kalau SUDAH BT lalu HIGH → pindah AUX seketika
// ============================================================================
#define BT_ENABLE_PIN            4
#define BT_STATUS_PIN            23
#define BT_AUTO_OFF_MS           BT_AUTO_OFF_IDLE_MS
#define BT_AUX_TO_BT_LOW_MS      AUX_TO_BT_LOW_MS


// ============================================================================
//  Relay & Speaker control
//  - RELAY utama default OFF di boot dingin
//  - SPEAKER_POWER_SWITCH_PIN dipindah ke pin bekas Voltmeter Switch (sesuai keputusan)
//  - SPEAKER_SELECTOR_PIN: HIGH=BIG, LOW=SMALL
// ============================================================================
#define RELAY_MAIN_PIN           14
#define RELAY_MAIN_ACTIVE_HIGH   1

#define SPEAKER_POWER_SWITCH_PIN 25
#define SPEAKER_SELECTOR_PIN     26
#define SPK_DEFAULT_BIG          1      // Default speaker = BIG (true)


// ============================================================================
//  Fan PWM (tanpa tachometer) + kurva AUTO 3-titik
//  - Mode: AUTO / CUSTOM / FAILSAFE (persist di NVS)
//  - Boot test opsional agar kipas “ngeroll” sebentar
// ============================================================================
#define FAN_PWM_PIN              32
#define FAN_PWM_CH               0
#define FAN_PWM_FREQ             25000
#define FAN_PWM_RES_BITS         10     // Duty 0..1023

// Boot test kipas (opsional)
#define FAN_BOOT_TEST_MS         800
#define FAN_BOOT_TEST_DUTY       900    // 0..1023

// Mode default & custom duty
// 0=AUTO, 1=CUSTOM, 2=FAILSAFE
#define FAN_DEFAULT_MODE         0
#define FAN_CUSTOM_DUTY          640    // dipakai jika mode CUSTOM

// Kurva AUTO (titik suhu → duty)
// Duty 0..1023; atur sesuai karakter kipas & heatsink
#define FAN_AUTO_T1_C            40.0f
#define FAN_AUTO_T2_C            60.0f
#define FAN_AUTO_T3_C            80.0f
#define FAN_AUTO_D1              200
#define FAN_AUTO_D2              650
#define FAN_AUTO_D3              1023

// Duty fallback bila FAILSAFE
#define FAN_FALLBACK_DUTY        900


// ============================================================================
//  Buzzer (LEDC)
//  - Klik pendek untuk setiap aksi UI dari panel
//  - Nada warning & error panjang saat ada error latched
// ============================================================================
#define BUZZER_PIN               33
#define BUZZER_PWM_CH            1
#define BUZZER_PWM_RES_BITS      10
#define BUZZER_PWM_BASE_FREQ     2000
#define BUZZER_DUTY_DEFAULT      360    // ~35% duty (0..1023)
#define BUZZER_DUTY_STRONG       480
#define BUZZER_WARNING_REPEAT_MS 2500
#define BUZZER_ERROR_REPEAT_MS   1200


// ============================================================================
//  PC Detect (opto) → Auto Power
//  - LOW = PC ON (aktif-low default modul opto)
//  - ON otomatis saat PC ON; OFF tertunda 5s saat PC OFF
//  - Diabaikan saat OTA (opsional)
// ============================================================================
#define PC_DETECT_PIN            34
#define PC_OFF_DELAY_MS          5000
#define PC_AUTO_IGNORE_DURING_OTA 1


// ============================================================================
//  Speaker Protector LED monitor (opto, hanya MONITOR)
//  - Jangan mematikan supply protector dari firmware; hanya laporkan fault
//  - Fault jika LED padam terus ≥ SPK_PROTECT_FAULT_MS
// ============================================================================
#define SPK_PROTECT_LED_PIN      39
#define SPK_PROTECT_ACTIVE_HIGH  1      // LED ON = normal
#define SPK_PROTECT_FAULT_MS     1500


// ============================================================================
//  Analyzer (ala Webspector) — I²S ADC internal
//  - Menggunakan ADC internal via driver I²S (BUKAN analogRead biasa)
//  - Satu pin analog: GPIO36 (ADC1_CH0) untuk input audio (mik analog)
//  - Tanpa “noise floor” kustom; window Hann + FFT 256 → 16 band log-spaced
//  - Nonaktif saat STANDBY untuk hemat beban
// ============================================================================
#define I2S_PORT                 I2S_NUM_0
#define I2S_USE_BUILTIN_ADC      1            // gunakan ADC internal
#define I2S_ADC_GPIO             36           // ADC1_CH0 (GPIO36)

// Parameter FFT/Analyzer
#define ANA_N                    256
#define ANA_FS_HZ                11025        // sample rate
#define ANA_UPDATE_MS            33           // ~30 FPS
#define ANA_F_LO_HZ              40
#define ANA_F_HI_HZ              5000
#define ANA_BANDS                16           // boleh 17 jika UI panel masih ada ruang


// ============================================================================
//  Telemetry pacing
//  - Saat ON   : realtime cepat (mis. 10 Hz)
//  - Saat STBY : 1 Hz, dipicu oleh SQW 1 Hz dari RTC
//  - Struktur JSON: {ver,type="telemetry",data:{time,fw_ver,ota_ready,smps_v,heat_c,inputs{bt,speaker},states{on,standby},errors[],an[]}}
// ============================================================================
#define TELEMETRY_HZ_ACTIVE      10
#define TELEMETRY_HZ_STANDBY     1


// ============================================================================
//  OTA via UART (Panel) — ukuran maksimum file .bin
//  - Lakukan verifikasi ukuran & checksum sebelum switch partisi
// ============================================================================
#define OTA_MAX_BIN_SIZE         (1024*1024)  // 1 MiB batas aman
#ifndef OTA_ENABLE
#define OTA_ENABLE               1
#endif


/*
Checklist cepat ketika ganti hardware:
- [ ] Pastikan tidak ada konflik pin (lihat semua #define PIN_*)
- [ ] Ubah R1_OHMS / R2_OHMS sesuai hasil pengukuran
- [ ] Sesuaikan kurva kipas jika karakter kipas berbeda
- [ ] Jika OLED/RTC/ADS alamatnya berbeda, ubah *_I2C_ADDR
- [ ] Jika Analyzer ingin 17 band, ubah ANA_BANDS ke 17 (panel juga disesuaikan)
- [ ] TELEMETRY_HZ_ACTIVE boleh dinaikkan jika link stabil
- [ ] OTA_MAX_BIN_SIZE harus ≤ ukuran partisi OTA di partitions.csv
*/
