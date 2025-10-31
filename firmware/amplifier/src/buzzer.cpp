#include "buzzer.h"
#include "config.h"

#include <driver/ledc.h>

struct BuzzStep {
  uint16_t freqHz;
  uint16_t durationMs;
  uint16_t duty;
};

struct BuzzPatternDef {
  const BuzzStep* steps;
  size_t          count;
  uint32_t        repeatIntervalMs;
};

static const BuzzStep PATTERN_BOOT[] = {
  {880, 90, BUZZER_DUTY_DEFAULT},
  {0,   30, 0},
  {1175, 90, BUZZER_DUTY_DEFAULT},
  {0,   30, 0},
  {1568, 120, BUZZER_DUTY_DEFAULT},
};

static const BuzzStep PATTERN_SHUTDOWN[] = {
  {1568, 90, BUZZER_DUTY_DEFAULT},
  {0,   30, 0},
  {1175, 90, BUZZER_DUTY_DEFAULT},
  {0,   30, 0},
  {880,  120, BUZZER_DUTY_DEFAULT},
};

static const BuzzStep PATTERN_BT[] = {
  {1568, 60, BUZZER_DUTY_DEFAULT},
  {0,   40, 0},
  {2093, 80, BUZZER_DUTY_DEFAULT},
};

static const BuzzStep PATTERN_AUX[] = {
  {1175, 60, BUZZER_DUTY_DEFAULT},
};

static const BuzzStep PATTERN_ACK[] = {
  {3000, 25, BUZZER_DUTY_STRONG},
};

static const BuzzStep PATTERN_WARNING[] = {
  {1175, 60, BUZZER_DUTY_DEFAULT},
};

static const BuzzStep PATTERN_ERROR[] = {
  {880, 70, BUZZER_DUTY_STRONG},
  {0,   100, 0},
  {880, 70, BUZZER_DUTY_STRONG},
};

static const BuzzPatternDef PATTERNS[] = {
  {nullptr, 0, 0},                                                     // NONE
  {PATTERN_BOOT,      sizeof(PATTERN_BOOT)      / sizeof(BuzzStep), 0},
  {PATTERN_SHUTDOWN,  sizeof(PATTERN_SHUTDOWN)  / sizeof(BuzzStep), 0},
  {PATTERN_BT,        sizeof(PATTERN_BT)        / sizeof(BuzzStep), 0},
  {PATTERN_AUX,       sizeof(PATTERN_AUX)       / sizeof(BuzzStep), 0},
  {PATTERN_ACK,       sizeof(PATTERN_ACK)       / sizeof(BuzzStep), 0},
  {PATTERN_WARNING,   sizeof(PATTERN_WARNING)   / sizeof(BuzzStep), BUZZER_WARNING_REPEAT_MS},
  {PATTERN_ERROR,     sizeof(PATTERN_ERROR)     / sizeof(BuzzStep), BUZZER_ERROR_REPEAT_MS},
};

static bool                     gEnabled        = true;
static const BuzzPatternDef*    gCurrent        = nullptr;
static size_t                   gStepIndex      = 0;
static uint32_t                 gStepEndMs      = 0;
static uint32_t                 gPatternStartMs = 0;
static uint32_t                 gNextCycleMs    = 0;
static bool                     gOutputActive   = false;

static bool                     gCustomActive   = false;
static uint32_t                 gCustomEndMs    = 0;

static inline uint32_t ms() { return millis(); }

static void buzzerOff() {
  ledcWrite(BUZZER_PWM_CH, 0);
  gOutputActive = false;
}

static void startStep(uint32_t now) {
  if (!gCurrent || gStepIndex >= gCurrent->count) {
    buzzerOff();
    if (gCurrent && gCurrent->repeatIntervalMs == 0) {
      gCurrent = nullptr;
    } else if (gCurrent) {
      gNextCycleMs = gPatternStartMs + gCurrent->repeatIntervalMs;
    }
    return;
  }

  const BuzzStep& step = gCurrent->steps[gStepIndex];
  gStepEndMs = now + step.durationMs;
  if (!gEnabled || step.freqHz == 0 || step.durationMs == 0 || step.duty == 0) {
    buzzerOff();
    gOutputActive = false;
    return;
  }

  ledcSetup(BUZZER_PWM_CH, step.freqHz, BUZZER_PWM_RES_BITS);
  ledcWrite(BUZZER_PWM_CH, step.duty);
  gOutputActive = true;
}

void buzzerInit() {
  ledcSetup(BUZZER_PWM_CH, BUZZER_PWM_BASE_FREQ, BUZZER_PWM_RES_BITS);
  ledcAttachPin(BUZZER_PIN, BUZZER_PWM_CH);
  buzzerOff();
  gCurrent = nullptr;
  gCustomActive = false;
  gEnabled = true;
}

void buzzSetEnabled(bool enabled) {
  gEnabled = enabled;
  if (!gEnabled) {
    gCurrent = nullptr;
    gCustomActive = false;
    buzzerOff();
  }
}

void buzzPattern(BuzzPatternId pattern) {
  if (!gEnabled) {
    return;
  }
  gCustomActive = false;
  size_t index = static_cast<size_t>(pattern);
  if (index >= (sizeof(PATTERNS) / sizeof(PATTERNS[0]))) {
    index = 0;
  }
  gCurrent = PATTERNS[index].steps ? &PATTERNS[index] : nullptr;
  gStepIndex = 0;
  gPatternStartMs = ms();
  gNextCycleMs = 0;
  if (!gCurrent) {
    buzzerOff();
    return;
  }
  startStep(gPatternStartMs);
}

void buzzStop() {
  gCurrent = nullptr;
  gCustomActive = false;
  buzzerOff();
}

void buzzTick(uint32_t now) {
  if (!gEnabled) {
    buzzerOff();
    return;
  }

  if (gCustomActive) {
    if (now >= gCustomEndMs) {
      gCustomActive = false;
      buzzerOff();
    }
    return;
  }

  if (!gCurrent) {
    return;
  }

  if (gStepIndex >= gCurrent->count) {
    if (gCurrent->repeatIntervalMs == 0) {
      gCurrent = nullptr;
      buzzerOff();
      return;
    }
    if (now >= gNextCycleMs) {
      gPatternStartMs = now;
      gStepIndex = 0;
      startStep(now);
    }
    return;
  }

  if (now >= gStepEndMs) {
    ++gStepIndex;
    if (gStepIndex < gCurrent->count) {
      startStep(now);
    } else {
      buzzerOff();
      if (gCurrent->repeatIntervalMs == 0) {
        gCurrent = nullptr;
      } else {
        gStepIndex = gCurrent->count; // wait for next cycle
        gNextCycleMs = gPatternStartMs + gCurrent->repeatIntervalMs;
      }
    }
  }
}

void buzzerCustom(uint32_t freqHz, uint16_t duty, uint16_t msDur) {
  if (!gEnabled) {
    return;
  }
  if (freqHz == 0 || msDur == 0 || duty == 0) {
    buzzStop();
    return;
  }
  gCurrent = nullptr;
  gCustomActive = true;
  gCustomEndMs = ms() + msDur;
  if (duty > ((1u << BUZZER_PWM_RES_BITS) - 1)) {
    duty = (1u << BUZZER_PWM_RES_BITS) - 1;
  }
  ledcSetup(BUZZER_PWM_CH, freqHz, BUZZER_PWM_RES_BITS);
  ledcWrite(BUZZER_PWM_CH, duty);
  gOutputActive = true;
}

bool buzzerIsActive() {
  if (!gEnabled) return false;
  if (gCustomActive) return true;
  if (!gCurrent) return false;
  return gStepIndex < gCurrent->count || gCurrent->repeatIntervalMs > 0;
}
