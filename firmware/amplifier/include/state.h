#pragma once
#include <Arduino.h>

// Mode kipas yang dipersist di NVS
enum class FanMode : uint8_t {
  AUTO     = 0,
  CUSTOM   = 1,
  FAILSAFE = 2
};

// Init & factory reset (hapus seluruh key NVS)
void     stateInit();
void     stateFactoryReset();

// -------- Persisted settings (NVS) --------
bool     stateSpeakerIsBig();
void     stateSetSpeakerIsBig(bool big);

bool     stateSpeakerPowerOn();
void     stateSetSpeakerPowerOn(bool on);

FanMode  stateGetFanMode();
void     stateSetFanMode(FanMode m);

uint16_t stateGetFanCustomDuty();            // 0..1023
void     stateSetFanCustomDuty(uint16_t d);

bool     stateSmpsBypass();
void     stateSetSmpsBypass(bool en);

float    stateSmpsCutoffV();
void     stateSetSmpsCutoffV(float v);

float    stateSmpsRecoveryV();
void     stateSetSmpsRecoveryV(float v);

bool     stateBtEnabled();
void     stateSetBtEnabled(bool en);

uint32_t stateBtAutoOffMs();
void     stateSetBtAutoOffMs(uint32_t ms);

// RTC sync rate-limit (epoch detik)
uint32_t stateLastRtcSync();
void     stateSetLastRtcSync(uint32_t t);

// -------- Runtime flags (tidak dipersist) --------
bool     powerIsOn();
bool     powerIsStandby();
void     powerSetOn(bool on);

// Safe mode compile-time flag (for diagnostics)
bool     stateSafeModeSoft();

// Tick ops (placeholder untuk housekeeping NVS)
void     stateTick();
