#pragma once

#include <Arduino.h>

enum class PanelOtaStatus {
  Idle,
  InProgress,
  Success,
  Failed,
};

void panelOtaInit();
void panelOtaTick(uint32_t nowMs);
bool panelOtaBegin(size_t expectedSize, uint32_t expectedCrc32);
int  panelOtaWrite(const uint8_t *data, size_t len);
bool panelOtaEnd(bool rebootAfter);
void panelOtaAbort();

PanelOtaStatus panelOtaStatus();
const char* panelOtaLastError();
bool panelOtaIsActive();
