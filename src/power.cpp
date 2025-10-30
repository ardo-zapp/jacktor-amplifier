#include "power.h"
#include "config.h"
#include "state.h"
#include "sensors.h"

#include <driver/ledc.h>

// -------------------- Static state --------------------
static bool   sRelayOn = false;
static bool   sRelayRequested = false;

static bool   sSpkBig  = false;
static bool   sSpkPwr  = false;

static bool   sBtEn    = true;
static bool   sBtMode  = false;  // true=BT, false=AUX

static bool   sOta     = false;

static bool   sSpkProtectOk = true;     // LED ON = OK (active-high)
static uint32_t protectLastChangeMs = 0;
static bool   protectFaultLatched = false;

static uint32_t btLastEnteredBtMs = 0;  // reset timer auto-off ketika masuk BT
static uint32_t btLastAuxMs       = 0;  // melacak lama berada di AUX

// AUX→BT membutuhkan LOW stabil >= 3s, jika SUDAH BT lalu HIGH → segera AUX
static uint32_t btLowSinceMs      = 0;

// PC detect
static bool     pcOn = false;
static uint32_t pcOffSchedAt = 0;       // jadwal OFF (now+delay) saat PC OFF

// Fan
static bool     fanBootTestDone = false;

// SMPS undervolt fault latch
static bool     smpsFaultLatched = false;
static bool     smpsCutActive    = false;


// -------------------- Helpers --------------------
static inline void _writeRelay(bool on) {
#if RELAY_MAIN_ACTIVE_HIGH
  digitalWrite(RELAY_MAIN_PIN, on ? HIGH : LOW);
#else
  digitalWrite(RELAY_MAIN_PIN, on ? LOW : HIGH);
#endif
  sRelayOn = on;
}

static inline void applyRelay(bool on) {
  _writeRelay(on);
  powerSetOn(on);
}

static inline bool _readBtStatusActiveLow() {
  // BT_STATUS_PIN: LOW = BT mode (aktif)
  return digitalRead(BT_STATUS_PIN) == LOW;
}

static inline bool _readSpkProtectLedActiveHigh() {
  // true jika LED ON (normal), false jika LED OFF (potensi fault)
#if SPK_PROTECT_ACTIVE_HIGH
  return digitalRead(SPK_PROTECT_LED_PIN) == HIGH;
#else
  return digitalRead(SPK_PROTECT_LED_PIN) == LOW;
#endif
}

static inline bool _readPcDetectActiveLow() {
#if PC_DETECT_ACTIVE_LOW
  return digitalRead(PC_DETECT_PIN) == LOW;
#else
  return digitalRead(PC_DETECT_PIN) == HIGH;
#endif
}

static inline uint32_t ms() { return millis(); }

// Fan duty 0..1023
static inline void fanWriteDuty(uint16_t duty) {
  if (duty > 1023) duty = 1023;
  ledcWrite(FAN_PWM_CH, duty);
}

// Linear map T → duty (t1..t2..t3)
static uint16_t fanCurveAuto(float tC) {
  if (isnan(tC)) {
    // sensor gagal → pilih duty aman sedang
    return (FAN_AUTO_D1 + FAN_AUTO_D2) / 2;
  }
  if (tC <= FAN_AUTO_T1_C) return FAN_AUTO_D1;
  if (tC >= FAN_AUTO_T3_C) return FAN_AUTO_D3;

  if (tC <= FAN_AUTO_T2_C) {
    // T1..T2
    float f = (tC - FAN_AUTO_T1_C) / (FAN_AUTO_T2_C - FAN_AUTO_T1_C);
    return (uint16_t)(FAN_AUTO_D1 + f * (FAN_AUTO_D2 - FAN_AUTO_D1));
  } else {
    // T2..T3
    float f = (tC - FAN_AUTO_T2_C) / (FAN_AUTO_T3_C - FAN_AUTO_T2_C);
    return (uint16_t)(FAN_AUTO_D2 + f * (FAN_AUTO_D3 - FAN_AUTO_D2));
  }
}

static void fanTick() {
  FanMode m = stateGetFanMode();
  uint16_t duty = 0;

  switch (m) {
    case FanMode::AUTO: {
      float t = getHeatsinkC();             // Celsius (NAN jika belum valid)
      duty = fanCurveAuto(t);
      break;
    }
    case FanMode::CUSTOM:
      duty = stateGetFanCustomDuty();
      break;
    case FanMode::FAILSAFE:
    default:
      duty = FAN_FALLBACK_DUTY;
      break;
  }

  fanWriteDuty(duty);
}

static void smpsProtectTick() {
  if (stateSmpsBypass()) {
    smpsCutActive = false;
    smpsFaultLatched = false;
    if (sRelayOn != sRelayRequested) {
      applyRelay(sRelayRequested);
    }
    return;
  }

  if (!sRelayRequested) {
    smpsCutActive = false;
    smpsFaultLatched = false;
    if (sRelayOn) {
      applyRelay(false);
    }
    return;
  }

  float v = getVoltageInstant();
  float cutoff = stateSmpsCutoffV();
  float recover = stateSmpsRecoveryV();

  if (!smpsCutActive && sRelayOn && v > 0.0f && v < cutoff) {
    smpsCutActive = true;
    smpsFaultLatched = true;
    applyRelay(false);
  }

  if (smpsCutActive && v >= recover) {
    smpsCutActive = false;
    smpsFaultLatched = false;
    if (sRelayRequested) {
      applyRelay(true);
    }
  }
}

// -------------------- Public API --------------------
void powerInit() {
  // Relay
  pinMode(RELAY_MAIN_PIN, OUTPUT);
  applyRelay(false);  // default OFF saat boot
  sRelayRequested = false;
  smpsCutActive = false;
  smpsFaultLatched = false;

  // Speaker control
  pinMode(SPEAKER_POWER_SWITCH_PIN, OUTPUT);
  pinMode(SPEAKER_SELECTOR_PIN, OUTPUT);

  // Default dari NVS
  sSpkBig = stateSpeakerIsBig();
  sSpkPwr = stateSpeakerPowerOn();
  digitalWrite(SPEAKER_SELECTOR_PIN, sSpkBig ? HIGH : LOW);
  digitalWrite(SPEAKER_POWER_SWITCH_PIN, sSpkPwr ? HIGH : LOW);

  // BT
  pinMode(BT_ENABLE_PIN, OUTPUT);
  pinMode(BT_STATUS_PIN, INPUT);
  sBtEn = stateBtEnabled();
  digitalWrite(BT_ENABLE_PIN, sBtEn ? HIGH : LOW);
  sBtMode = _readBtStatusActiveLow();
  uint32_t now = ms();
  btLastEnteredBtMs = sBtMode ? now : 0;
  btLastAuxMs       = sBtMode ? 0   : now;
  btLowSinceMs      = sBtMode ? now : 0;

  // PC detect
  pinMode(PC_DETECT_PIN, INPUT);
  pcOn = _readPcDetectActiveLow();
  pcOffSchedAt = 0;

  // Speaker protector LED monitor
  pinMode(SPK_PROTECT_LED_PIN, INPUT);
  sSpkProtectOk = _readSpkProtectLedActiveHigh();
  protectLastChangeMs = now;
  protectFaultLatched = false;

  // Fan PWM
  ledcSetup(FAN_PWM_CH, FAN_PWM_FREQ, FAN_PWM_RES_BITS);
  ledcAttachPin(FAN_PWM_PIN, FAN_PWM_CH);

  // Boot test fan opsional
  if (FAN_BOOT_TEST_ENABLE) {
    fanWriteDuty(FAN_BOOT_TEST_DUTY);
    delay(FAN_BOOT_TEST_MS);
  }
  fanBootTestDone = true;
  fanTick();
}

void powerTick(uint32_t now) {
  // ---------------- Fan control ----------------
  fanTick();
  smpsProtectTick();

  // ---------------- Speaker protector monitor ----------------
  bool ok = _readSpkProtectLedActiveHigh();
  if (ok != sSpkProtectOk) {
    sSpkProtectOk = ok;
    protectLastChangeMs = now;
  } else {
    // tetap sama; cek fault
    if (!sSpkProtectOk && !protectFaultLatched) {
      if (now - protectLastChangeMs >= SPK_PROTECT_FAULT_MS) {
        protectFaultLatched = true; // latched fault; clear ketika LED kembali ON
      }
    }
    if (sSpkProtectOk && protectFaultLatched) {
      // clear fault saat kembali normal
      protectFaultLatched = false;
    }
  }

  // ---------------- BT logic (real-time) ----------------
  if (sBtEn) {
    bool lowNow = _readBtStatusActiveLow(); // LOW=BT
    if (lowNow) {
      if (!sBtMode) {
        // sedang AUX → kandidat menuju BT
        if (btLowSinceMs == 0) btLowSinceMs = now;
        if ((now - btLowSinceMs) >= BT_AUX_TO_BT_LOW_MS) {
          sBtMode = true;                   // masuk BT
          btLastEnteredBtMs = now;
          btLastAuxMs = 0;
        }
      } else {
        // tetap BT; reset AUX timer
        btLastEnteredBtMs = (btLastEnteredBtMs == 0) ? now : btLastEnteredBtMs;
        btLastAuxMs = 0;
      }
    } else {
      // HIGH sekarang
      btLowSinceMs = 0;
      if (sBtMode) {
        // SUDAH BT → begitu HIGH → segera AUX
        sBtMode = false;
        btLastAuxMs = now;
      } else {
        // tetap AUX
        if (btLastAuxMs == 0) btLastAuxMs = now;
      }
    }

    // Auto-off modul BT jika terlalu lama di AUX
    if (!sBtMode && stateBtAutoOffMs() > 0) {
      if (btLastAuxMs != 0 && (now - btLastAuxMs) >= stateBtAutoOffMs()) {
        powerSetBtEnabled(false); // matikan modul
      }
    }
  }

  // ---------------- Auto power via PC detect ----------------
  bool pcNow = _readPcDetectActiveLow();
  if (pcNow != pcOn) {
    pcOn = pcNow;
    if (!sOta) {
      if (pcOn) {
        // PC ON → power ON
        powerSetMainRelay(true);
      } else {
        // PC OFF → jadwalkan OFF setelah delay
        pcOffSchedAt = now + PC_OFF_DELAY_MS;
      }
    }
  }
  if (!pcOn && pcOffSchedAt != 0 && now >= pcOffSchedAt) {
    if (!sOta) {
      powerSetMainRelay(false);
    }
    pcOffSchedAt = 0;
  }
}

// ---------------- Relay ----------------
void powerSetMainRelay(bool on) {
  sRelayRequested = on;
  if (!on) {
    smpsCutActive = false;
    smpsFaultLatched = false;
  }
  applyRelay(on);
}
bool powerMainRelay() { return sRelayOn; }

// ---------------- Speaker ----------------
void powerSetSpeakerSelect(bool big) {
  sSpkBig = big;
  digitalWrite(SPEAKER_SELECTOR_PIN, big ? HIGH : LOW);
  stateSetSpeakerIsBig(big);
}
bool powerGetSpeakerSelectBig() { return sSpkBig; }

void powerSetSpeakerPower(bool on) {
  sSpkPwr = on;
  digitalWrite(SPEAKER_POWER_SWITCH_PIN, on ? HIGH : LOW);
  stateSetSpeakerPowerOn(on);
}
bool powerGetSpeakerPower() { return sSpkPwr; }

// ---------------- Bluetooth ----------------
void powerSetBtEnabled(bool en) {
  sBtEn = en;
  digitalWrite(BT_ENABLE_PIN, en ? HIGH : LOW);
  stateSetBtEnabled(en);
  // Reset timer ketika baru diaktifkan kembali
  uint32_t now = ms();
  if (en) {
    // baca status aktual
    sBtMode = _readBtStatusActiveLow();
    btLowSinceMs = sBtMode ? now : 0;
    btLastEnteredBtMs = sBtMode ? now : 0;
    btLastAuxMs = sBtMode ? 0 : now;
  }
}
bool powerBtEnabled() { return sBtEn; }
bool powerBtMode()    { return sBtMode; } // true=BT, false=AUX

// ---------------- OTA guard ----------------
void powerSetOtaActive(bool on) {
  sOta = on;
  // Saat OTA aktif → auto power via PC detect diabaikan (dikelola di tick)
}

// ---------------- Protector Fault ----------------
bool powerSpkProtectFault() {
  return protectFaultLatched;
}

// ---------------- Input mode string ----------------
const char* powerInputModeStr() {
  return sBtMode ? "bt" : "aux";
}