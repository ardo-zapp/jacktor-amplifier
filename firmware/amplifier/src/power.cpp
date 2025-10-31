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

static bool   sBtEn    = false;
static bool   sBtHwOn  = false;
static bool   sBtMode  = false;  // true=BT, false=AUX

static bool   sOta     = false;
static bool   safeModeActive = false;

static bool   sSpkProtectOk = true;     // LED ON = OK (active-high)
static uint32_t protectLastChangeMs = 0;
static bool   protectFaultLatched = false;
static bool   protectFaultLogged = false;

static uint32_t btLastEnteredBtMs = 0;  // reset timer auto-off ketika masuk BT
static uint32_t btLastAuxMs       = 0;  // melacak lama berada di AUX

// AUX→BT membutuhkan LOW stabil >= 3s, jika SUDAH BT lalu HIGH → segera AUX
static uint32_t btLowSinceMs      = 0;

// PC detect
static bool     pcOn = false;
static bool     pcRaw = false;
static uint32_t pcLastRawMs = 0;
static uint32_t pcGraceUntilMs = 0;
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
  int val = digitalRead(BT_STATUS_PIN);
  if (BT_STATUS_ACTIVE_LOW) {
    return val == LOW;
  }
  return val == HIGH;
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

static void applyBtHardware() {
  bool shouldOn = sBtEn && powerIsOn() && !safeModeActive;
  if (shouldOn != sBtHwOn) {
    digitalWrite(BT_ENABLE_PIN, shouldOn ? HIGH : LOW);
    sBtHwOn = shouldOn;
  }
}

static void smpsProtectTick() {
  if (!FEAT_SMPS_PROTECT_ENABLE) {
    smpsCutActive = false;
    smpsFaultLatched = false;
    if (sRelayOn != sRelayRequested) {
      applyRelay(sRelayRequested);
    }
    return;
  }

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
  safeModeActive = stateSafeModeSoft();

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
  sSpkPwr = safeModeActive ? false : stateSpeakerPowerOn();
  digitalWrite(SPEAKER_SELECTOR_PIN, sSpkBig ? HIGH : LOW);
  digitalWrite(SPEAKER_POWER_SWITCH_PIN, sSpkPwr ? HIGH : LOW);

  // BT
  pinMode(BT_ENABLE_PIN, OUTPUT);
  pinMode(BT_STATUS_PIN, INPUT);
  sBtEn = stateBtEnabled();
  uint32_t now = ms();
  sBtMode = (FEAT_BT_AUTOSWITCH_AUX && sBtHwOn) ? _readBtStatusActiveLow() : false;
  btLastEnteredBtMs = sBtMode ? now : 0;
  btLastAuxMs       = sBtMode ? 0   : now;
  btLowSinceMs      = sBtMode ? now : 0;

  // PC detect
  pinMode(PC_DETECT_PIN, PC_DETECT_INPUT_PULL);
  pcRaw = _readPcDetectActiveLow();
  pcOn = pcRaw;
  pcLastRawMs = now;
  pcGraceUntilMs = now + PC_DETECT_GRACE_MS;
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
  if (FEAT_FAN_BOOT_TEST) {
    fanWriteDuty(FAN_BOOT_TEST_DUTY);
    delay(FAN_BOOT_TEST_MS);
  }
  fanBootTestDone = true;
  fanTick();

  applyBtHardware();

  if (safeModeActive) {
    fanWriteDuty(0);
    digitalWrite(SPEAKER_POWER_SWITCH_PIN, LOW);
    sSpkPwr = false;
#if LOG_ENABLE
    LOGF("[SAFE] Jacktor Audio safe-mode active\n");
#endif
    commsLog("warn", "safe_mode");
  }
}

void powerTick(uint32_t now) {
  // ---------------- Fan control ----------------
  fanTick();
  if (safeModeActive) {
    fanWriteDuty(0);
  }
  smpsProtectTick();

  // ---------------- Speaker protector monitor ----------------
  bool ok = _readSpkProtectLedActiveHigh();
  if (ok != sSpkProtectOk) {
    sSpkProtectOk = ok;
    protectLastChangeMs = now;
  } else {
    if (!sSpkProtectOk && !protectFaultLatched) {
      if (now - protectLastChangeMs >= SPK_PROTECT_FAULT_MS) {
        protectFaultLatched = true;
      }
    }
    if (sSpkProtectOk && protectFaultLatched) {
      protectFaultLatched = false;
    }
  }
  if (protectFaultLatched != protectFaultLogged) {
    protectFaultLogged = protectFaultLatched;
#if LOG_ENABLE
    LOGF(protectFaultLatched ? "[PROTECT] speaker_fail\n" : "[PROTECT] speaker_clear\n");
#endif
  }

  // ---------------- BT logic (real-time) ----------------
  if (FEAT_BT_AUTOSWITCH_AUX && sBtHwOn) {
    bool lowNow = _readBtStatusActiveLow();
    if (lowNow) {
      if (!sBtMode) {
        if (btLowSinceMs == 0) btLowSinceMs = now;
        if ((now - btLowSinceMs) >= AUX_TO_BT_LOW_MS) {
          sBtMode = true;
          btLastEnteredBtMs = now;
          btLastAuxMs = 0;
        }
      } else {
        btLastEnteredBtMs = (btLastEnteredBtMs == 0) ? now : btLastEnteredBtMs;
        btLastAuxMs = 0;
      }
    } else {
      btLowSinceMs = 0;
      if (sBtMode) {
        sBtMode = false;
        btLastAuxMs = now;
      } else if (btLastAuxMs == 0) {
        btLastAuxMs = now;
      }
    }
  }

  // Auto-off modul BT jika terlalu lama di AUX
  if (sBtEn && sBtHwOn) {
    uint32_t idleMs = stateBtAutoOffMs();
    if (idleMs > 0 && !sBtMode && btLastAuxMs != 0) {
      if ((now - btLastAuxMs) >= idleMs) {
        powerSetBtEnabled(false);
      }
    }
  }

  // ---------------- Auto power via PC detect ----------------
  if (FEAT_PC_DETECT_ENABLE && !sOta && !safeModeActive) {
    bool raw = _readPcDetectActiveLow();
    if (raw != pcRaw) {
      pcRaw = raw;
      pcLastRawMs = now;
    }
    if ((now - pcLastRawMs) >= PC_DETECT_DEBOUNCE_MS) {
      if (raw != pcOn) {
        pcOn = raw;
        if (pcOn) {
          pcGraceUntilMs = now + PC_DETECT_GRACE_MS;
          powerSetMainRelay(true);
        } else {
          pcOffSchedAt = now + PC_DETECT_GRACE_MS;
        }
      }
    }
    if (!pcOn && pcOffSchedAt != 0 && now >= pcOffSchedAt && now >= pcGraceUntilMs) {
      powerSetMainRelay(false);
      pcOffSchedAt = 0;
    }
  } else {
    pcOffSchedAt = 0;
  }

  applyBtHardware();
}

// ---------------- Relay ----------------
void powerSetMainRelay(bool on) {
  sRelayRequested = on;
  if (safeModeActive) {
    on = false;
  }
  if (!on) {
    smpsCutActive = false;
    smpsFaultLatched = false;
  }
  applyRelay(on);
  if (on && FEAT_PC_DETECT_ENABLE) {
    pcGraceUntilMs = ms() + PC_DETECT_GRACE_MS;
  }
  applyBtHardware();
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
  bool hw = safeModeActive ? false : on;
  digitalWrite(SPEAKER_POWER_SWITCH_PIN, hw ? HIGH : LOW);
  stateSetSpeakerPowerOn(on);
}
bool powerGetSpeakerPower() { return sSpkPwr; }

// ---------------- Bluetooth ----------------
void powerSetBtEnabled(bool en) {
  sBtEn = en;
  stateSetBtEnabled(en);
  // Reset timer ketika baru diaktifkan kembali
  uint32_t now = ms();
  applyBtHardware();
  if (en && sBtHwOn) {
    sBtMode = _readBtStatusActiveLow();
    btLowSinceMs = sBtMode ? now : 0;
    btLastEnteredBtMs = sBtMode ? now : 0;
    btLastAuxMs = sBtMode ? 0 : now;
  }
  if (!en) {
    btLowSinceMs = 0;
    btLastEnteredBtMs = 0;
    btLastAuxMs = now;
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