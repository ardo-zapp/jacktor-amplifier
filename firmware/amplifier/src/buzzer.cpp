#include "buzzer.h"
#include "config.h"
#include <driver/ledc.h>

// Internal state for non-blocking pattern scheduler
static bool     sActive = false;
static uint32_t sUntilMs = 0;
static uint8_t  sRepeat  = 0;
static uint32_t sFreqHz  = BUZZER_PWM_FREQ;
static uint16_t sDuty    = 0;

// Simple helpers
static inline uint32_t ms() { return millis(); }

// Apply current tone on LEDC
static void buzzerApply(uint32_t freqHz, uint16_t duty) {
  if (duty > ((1u << BUZZER_PWM_RES_BITS) - 1)) {
    duty = (1u << BUZZER_PWM_RES_BITS) - 1;
  }
  // Reconfigure channel frequency when needed (custom tone)
  ledcSetup(BUZZER_PWM_CH, freqHz, BUZZER_PWM_RES_BITS);
  ledcWrite(BUZZER_PWM_CH, duty);
}

// Hard off
static void buzzerOff() {
  ledcWrite(BUZZER_PWM_CH, 0);
  sActive = false;
  sUntilMs = 0;
  sRepeat  = 0;
}

void buzzerInit() {
  ledcSetup(BUZZER_PWM_CH, BUZZER_PWM_FREQ, BUZZER_PWM_RES_BITS);
  ledcAttachPin(BUZZER_PIN, BUZZER_PWM_CH);
  buzzerOff();
}

void buzzerTick(uint32_t now) {
  if (!sActive) return;

  if (now >= sUntilMs) {
    if (sRepeat > 0) {
      // Next repeat for error tone (same freq/duty/duration)
      sRepeat--;
      sUntilMs = now + BUZZER_ERR_MS;
      buzzerApply(sFreqHz, sDuty);
    } else {
      buzzerOff();
    }
  }
}

void buzzerClick() {
  sFreqHz = BUZZER_PWM_FREQ;
  sDuty   = BUZZER_DUTY_CLICK;
  sRepeat = 0;
  sUntilMs = ms() + BUZZER_CLICK_MS;
  sActive = true;
  buzzerApply(sFreqHz, sDuty);
}

void buzzerWarning() {
  sFreqHz = BUZZER_PWM_FREQ;
  sDuty   = BUZZER_DUTY_WARN;
  sRepeat = 0;
  sUntilMs = ms() + BUZZER_WARNING_MS;
  sActive = true;
  buzzerApply(sFreqHz, sDuty);
}

void buzzerError() {
  sFreqHz = BUZZER_PWM_FREQ;
  sDuty   = BUZZER_DUTY_ERR;
  sRepeat = (BUZZER_ERROR_REPEAT > 0) ? (BUZZER_ERROR_REPEAT - 1) : 0;
  sUntilMs = ms() + BUZZER_ERR_MS;
  sActive = true;
  buzzerApply(sFreqHz, sDuty);
}

void buzzerErrorPattern() {
  buzzerError();
}

void buzzerCustom(uint32_t freqHz, uint16_t duty, uint16_t msDur) {
  if (freqHz == 0 || msDur == 0 || duty == 0) {
    buzzerOff();
    return;
  }
  sFreqHz = freqHz;
  sDuty   = duty;
  sRepeat = 0;
  sUntilMs = ms() + msDur;
  sActive = true;
  buzzerApply(sFreqHz, sDuty);
}

void buzzerStop() {
  buzzerOff();
}

bool buzzerIsActive() {
  return sActive;
}