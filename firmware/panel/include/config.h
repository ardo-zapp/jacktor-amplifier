#pragma once

#include <Arduino.h>

// --- Mapping pin (pastikan sesuai hardware)
#define PIN_UART2_TX       17
#define PIN_UART2_RX       16
#define PIN_AMP_EN         23      // LOW aktif (reset amplifier)
#define PIN_AMP_GPIO0      27      // LOW aktif (flash amplifier)
#define PIN_TRIG_PWR       32      // LOW=tekan tombol power Android
#define PIN_USB_ID         13      // LOW=ID→GND (minta mode host)
#define PIN_VBUS_SNS       34      // TRUE bila 5V hadir
#define PIN_LED_R          25
#define PIN_LED_G          26
#define PIN_I2C_SDA        21
#define PIN_I2C_SCL        22

// --- OTG ID adaptif + backoff + handshake
#define OTG_PULSE_LOW_MS            800
static const uint32_t OTG_BACKOFF_SCHEDULE_MS[] = {1000, 2000, 4000, 8000, 15000, 30000};
static const size_t   OTG_BACKOFF_LEN          = 6;
#define OTG_MAX_PULSES_PER_CYCLE    6
#define OTG_MAX_PROBE_DURATION_MS   60000     // 60 s batas total percobaan
#define OTG_COOLDOWN_MS             (5 * 60 * 1000)
#define OTG_VBUS_DEBOUNCE_MS        500
#define OTG_GRACE_AFTER_VBUS_MS     2000
#define OTG_HANDSHAKE_TIMEOUT_MS    10000
#define OTG_VBUS_LOSS_MS            2000      // drop vbus dianggap putus bila >2s

// --- Kebijakan wake Android (GPIO32)
#define POWER_WAKE_ON_BOOT          1         // pulse sekali saat panel boot
#define POWER_WAKE_ON_FAILURE       1         // fallback bila VBUS gagal muncul
#define POWER_WAKE_PULSE_MS         1000
#define POWER_WAKE_GRACE_MS         3000      // tunggu setelah pulse power
#define POWER_WAKE_MAX_PER_EVENT    1         // batasi per siklus event
#define POWER_WAKE_COOLDOWN_MS      (15 * 60 * 1000)

// --- Serial/bridge parameter
#define HOST_SERIAL_BAUD            921600
#define AMP_SERIAL_BAUD             921600
#define BRIDGE_MAX_FRAME            512

// --- Handshake JSON
// UI host (desktop/android) wajib kirim {"type":"hello","who":"android|desktop","app_ver":"x.y.z","schema_ver":"1.1"}
// Panel balas {"type":"ack","ok":true,"msg":"hello_ack","host":"ok"}
