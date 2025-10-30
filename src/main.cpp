#include <HardwareSerial.h>
HardwareSerial espSerial(2);
#include "main.h"
#include "config.h"

#include <Wire.h>
#include <cstring>

#include "state.h"    // NVS, rev/hash, default speaker BIG
#include "sensors.h"  // RTC, ADS1115, DS18B20, Analyzer (I2S)
#include "power.h"    // Relay, speaker power/selector, fan PWM, auto PC
#include "comms.h"    // UART link + telemetry + command handler (incl. buzz JSON)
#include "buzzer.h"   // non-blocking scheduler
#include "ui.h"       // OLED kecil (standby clock, status+VU saat ON)
#include "ota.h"      // OTA over UART (verifikasi .bin, reboot)

#if LOG_ENABLE
  #define LOGF(...)  do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #define LOGF(...)  do {} while (0)
#endif

static bool gPowerInitDone = false;

static inline uint8_t relayOffLevel() {
  return RELAY_MAIN_ACTIVE_HIGH ? LOW : HIGH;
}

static void ensureMainRelayOffRaw() {
  pinMode(RELAY_MAIN_PIN, OUTPUT);
  digitalWrite(RELAY_MAIN_PIN, relayOffLevel());
}

static void buzzerDoubleBeep() {
  for (int i = 0; i < 2; ++i) {
    buzzerWarning();
    delay(BUZZER_WARNING_MS + 60);
    buzzerTick(millis());
    buzzerStop();
    delay(60);
  }
}

void appPerformFactoryReset(const char* subtitle, const char* src) {
  uiShowFactoryReset(subtitle, 0);
  buzzerDoubleBeep();
  stateFactoryReset();
  if (gPowerInitDone) {
    powerSetMainRelay(false);
  } else {
    ensureMainRelayOffRaw();
  }
  commsLogFactoryReset(src);
  delay(1500);
  ESP.restart();
  while (true) {
    delay(100);
  }
}

static void checkManualFactoryResetCombo() {
#if COMBO_FACTORY_RESET
  delay(100);
  bool bootHeld  = (digitalRead(BTN_BOOT_PIN) == LOW);
  bool powerHeld = (digitalRead(SPEAKER_POWER_SWITCH_PIN) == LOW);
  if (bootHeld && powerHeld) {
    delay(1000);
    if (digitalRead(BTN_BOOT_PIN) == LOW && digitalRead(SPEAKER_POWER_SWITCH_PIN) == LOW) {
      appPerformFactoryReset("FACTORY RESET", "manual");
    }
  }
#endif
}

// ---- Init -------------------------------------------------------------------
void appInit() {
#if LOG_ENABLE
  Serial.begin(LOG_BAUD);
  delay(20);
  LOGF("\n[%s] %s v%s\n", "BOOT", FW_NAME, FW_VERSION);
#endif

  // I2C (RTC + ADS1115)
  Wire.begin(I2C_SDA, I2C_SCL);

  ensureMainRelayOffRaw();

#if COMBO_FACTORY_RESET
  pinMode(BTN_BOOT_PIN, INPUT_PULLUP);
  pinMode(SPEAKER_POWER_SWITCH_PIN, INPUT_PULLUP);
#endif

  // Subsystems
  buzzerInit();
  stateInit();
  commsInit();       // UART2 link ke Panel
  uiInit();
  checkManualFactoryResetCombo();

  sensorsInit();     // ADS1115, DS18B20, RTC, Analyzer (I2S/Task core0 jika diaktifkan)
  powerInit();       // relay default OFF; fan PWM ready
  gPowerInitDone = true;
  uiShowBoot(UI_BOOT_HOLD_MS);
#if OTA_ENABLE
  otaInit();         // siapkan channel OTA via UART (verif image)
#endif

  LOGF("[INIT] done.\n");
}

// ---- Loop (dipanggil dari loop()) ------------------------------------------
void appTick() {
  static uint32_t lastUi = 0;
  static bool lastAnalyzerEnabled = true;
  const uint32_t now = millis();

  // 1) Service sensor/fan/power/PC auto
  sensorsTick(now);      // update suhu (heatsink), volt (ADS), analyzer (I2S), VU
  powerTick(now);        // fan curve, proteksi SMPS (bypass via config), auto PC ON/OFF

  bool analyzerShouldRun = powerIsOn();
  if (analyzerShouldRun != lastAnalyzerEnabled) {
    sensorsSetAnalyzerEnabled(analyzerShouldRun);
    lastAnalyzerEnabled = analyzerShouldRun;
  }

  // 2) Telemetry & command link
  //    - Mode ON: rate = TELEMETRY_HZ_ACTIVE
  //    - Mode STANDBY: rate = TELEMETRY_HZ_STANDBY, sinkron lewat SQW 1 Hz
  bool sqw = sensorsSqwConsumeTick();
  commsTick(now, sqw);   // internal: kirim Telemetry, proses RX JSON (buzz, setConfig, OTA, dll.)

#if OTA_ENABLE
  otaTick(now);          // non-blocking OTA state machine
#endif

  // Update UI context info
  uiSetInputStatus(powerBtMode(), powerGetSpeakerSelectBig());
  if (sqw) {
    char iso[20];
    if (sensorsGetTimeISO(iso, sizeof(iso)) && strlen(iso) >= 19) {
      uiSetClock(&iso[11]);
    }
  }

  // 3) UI kecil di amplifier
  if (now - lastUi >= 33) {
    lastUi = now;
    uiTick(now);            // standby: jam besar; ON: status bar (HH:MM BT/AUX | °C + Volt) + VU
  }

  // 4) Buzzer & NVS
  buzzerTick(now);
  stateTick();
}

// ---- Helpers ----------------------------------------------------------------
void appSafeReboot() {
  LOGF("[SYS] reboot...\n");
  delay(50);
  ESP.restart();
}

// ---- Arduino entry ----------------------------------------------------------
void setup()  { appInit(); }
void loop()   { appTick(); }