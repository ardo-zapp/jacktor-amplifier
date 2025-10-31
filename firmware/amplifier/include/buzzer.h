#pragma once
#include <Arduino.h>

enum class BuzzPatternId : uint8_t {
  NONE = 0,
  BOOT,
  SHUTDOWN,
  ENTER_BT,
  ENTER_AUX,
  ACK,
  WARNING_LOOP,
  ERROR_LOOP
};

// Init LEDC untuk buzzer (mengambil konstanta dari config.h)
void buzzerInit();

// Aktif/nonaktifkan buzzer global (ketika off → output 0 dan pattern diabaikan)
void buzzSetEnabled(bool enabled);

// Mainkan pattern preset (lihat BuzzPatternId). buzzPattern(BuzzPatternId::NONE) mematikan pattern aktif.
void buzzPattern(BuzzPatternId pattern);

// Panggil rutin di loop
void buzzTick(uint32_t now);

// Penghenti paksa (mematikan output buzzer)
void buzzStop();

// Nada kustom dari panel (freq Hz, duty 0..1023, durasi ms).
// Duty diabaikan jika > resolusi; akan diklip ke 0..1023.
void buzzerCustom(uint32_t freqHz, uint16_t duty, uint16_t ms);

// Status
bool buzzerIsActive();
