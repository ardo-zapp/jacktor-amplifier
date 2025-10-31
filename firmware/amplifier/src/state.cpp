#include "state.h"
#include "config.h"
#include <Preferences.h>

// Namespace NVS tunggal untuk semua setting
static Preferences nv;

// Runtime flags (tidak dipersist)
static bool gOn = false;
static bool gStby = true;

// Cached persisted values (diisi saat stateInit)
static bool     sSpeakerBig;
static bool     sSpeakerPwr;
static FanMode  sFanMode;
static uint16_t sFanDuty;

static bool     sSmpsBypass;
static float    sSmpsCutV;
static float    sSmpsRecV;

static bool     sBtEn;
static uint32_t sBtOffMs;

static uint32_t sRtcSyncTs;

// Helpers untuk key
static constexpr const char* NS               = "jacktor_audio";
static constexpr const char* K_SPK_BIG        = "spk_big";
static constexpr const char* K_SPK_PWR        = "spk_pwr";
static constexpr const char* K_FAN_MODE       = "fan_mode";
static constexpr const char* K_FAN_DUTY       = "fan_duty";
static constexpr const char* K_SMPS_BYPASS    = "smps_bypass";
static constexpr const char* K_SMPS_CUT       = "smps_cut";
static constexpr const char* K_SMPS_REC       = "smps_rec";
static constexpr const char* K_BT_EN          = "bt_en";
static constexpr const char* K_BT_OFFMS       = "bt_off";
static constexpr const char* K_RTC_SYNC       = "rtc_sync";

static void loadFromNvs() {
  sSpeakerBig  = nv.getBool  (K_SPK_BIG,  SPK_DEFAULT_BIG);
  sSpeakerPwr  = nv.getBool  (K_SPK_PWR,  false);

  sFanMode     = (FanMode) nv.getUChar (K_FAN_MODE, (uint8_t)FanMode::AUTO);
  sFanDuty     = nv.getUShort(K_FAN_DUTY, FAN_CUSTOM_DUTY);

  sSmpsBypass  = nv.getBool  (K_SMPS_BYPASS, SMPS_PROTECT_BYPASS);
  sSmpsCutV    = nv.getFloat (K_SMPS_CUT,    SMPS_CUT_V);
  sSmpsRecV    = nv.getFloat (K_SMPS_REC,    SMPS_REC_V);

  sBtEn        = nv.getBool  (K_BT_EN,   FEAT_BT_ENABLE_AT_BOOT != 0);
  sBtOffMs     = nv.getULong (K_BT_OFFMS, BT_AUTO_OFF_IDLE_MS);

  sRtcSyncTs   = nv.getULong (K_RTC_SYNC, 0);
}

void stateInit() {
  nv.begin(NS, /*readOnly=*/false);
  loadFromNvs();

  // Default runtime: standby di boot dingin
  gOn = false;
  gStby = true;
}

void stateFactoryReset() {
  nv.clear();
  loadFromNvs();
}

// ----------------- Persisted setters/getters -----------------
bool stateSpeakerIsBig() { return sSpeakerBig; }
void stateSetSpeakerIsBig(bool big) {
  sSpeakerBig = big;
  nv.putBool(K_SPK_BIG, big);
}

bool stateSpeakerPowerOn() { return sSpeakerPwr; }
void stateSetSpeakerPowerOn(bool on) {
  sSpeakerPwr = on;
  nv.putBool(K_SPK_PWR, on);
}

FanMode stateGetFanMode() { return sFanMode; }
void stateSetFanMode(FanMode m) {
  sFanMode = m;
  nv.putUChar(K_FAN_MODE, (uint8_t)m);
}

uint16_t stateGetFanCustomDuty() { return sFanDuty; }
void     stateSetFanCustomDuty(uint16_t d) {
  if (d > 1023) d = 1023;
  sFanDuty = d;
  nv.putUShort(K_FAN_DUTY, d);
}

bool  stateSmpsBypass() { return sSmpsBypass; }
void  stateSetSmpsBypass(bool en) {
  sSmpsBypass = en;
  nv.putBool(K_SMPS_BYPASS, en);
}

float stateSmpsCutoffV() { return sSmpsCutV; }
void  stateSetSmpsCutoffV(float v) {
  sSmpsCutV = v;
  nv.putFloat(K_SMPS_CUT, v);
}

float stateSmpsRecoveryV() { return sSmpsRecV; }
void  stateSetSmpsRecoveryV(float v) {
  sSmpsRecV = v;
  nv.putFloat(K_SMPS_REC, v);
}

bool  stateBtEnabled() { return sBtEn; }
void  stateSetBtEnabled(bool en) {
  sBtEn = en;
  nv.putBool(K_BT_EN, en);
}

uint32_t stateBtAutoOffMs() { return sBtOffMs; }
void     stateSetBtAutoOffMs(uint32_t ms) {
  sBtOffMs = ms;
  nv.putULong(K_BT_OFFMS, ms);
}

uint32_t stateLastRtcSync() { return sRtcSyncTs; }
void     stateSetLastRtcSync(uint32_t t) {
  sRtcSyncTs = t;
  nv.putULong(K_RTC_SYNC, t);
}

// ----------------- Runtime power flags -----------------
bool powerIsOn()       { return gOn; }
bool powerIsStandby()  { return gStby; }
void powerSetOn(bool on) {
  gOn = on;
  gStby = !on;
}

void stateTick() {
  // Placeholder untuk future housekeeping; saat ini tidak ada pekerjaan periodik.
}

bool stateSafeModeSoft() {
  return SAFE_MODE_SOFT != 0;
}
