#include "ota_panel.h"

#include <Update.h>

static PanelOtaStatus sStatus = PanelOtaStatus::Idle;
static String         sError;
static size_t         sExpectedSize = 0;
static uint32_t       sExpectedCrc  = 0;
static size_t         sWritten      = 0;
static uint32_t       sRunningCrc   = 0;
static bool           sRebootPending = false;
static uint32_t       sRebootAtMs    = 0;

static uint32_t crc32_update(uint32_t crc, const uint8_t *buf, size_t len) {
  static uint32_t table[256];
  static bool tableInit = false;
  if (!tableInit) {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int j = 0; j < 8; ++j) {
        if (c & 1U) {
          c = 0xEDB88320UL ^ (c >> 1);
        } else {
          c >>= 1;
        }
      }
      table[i] = c;
    }
    tableInit = true;
  }

  crc = ~crc;
  for (size_t i = 0; i < len; ++i) {
    crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
  }
  return ~crc;
}

static void resetState() {
  sStatus = PanelOtaStatus::Idle;
  sError = "";
  sExpectedSize = 0;
  sExpectedCrc = 0;
  sWritten = 0;
  sRunningCrc = 0;
  sRebootPending = false;
  sRebootAtMs = 0;
}

void panelOtaInit() {
  resetState();
}

void panelOtaTick(uint32_t nowMs) {
  if (sRebootPending && nowMs >= sRebootAtMs) {
    sRebootPending = false;
    delay(50);
    ESP.restart();
  }
}

bool panelOtaIsActive() {
  return sStatus == PanelOtaStatus::InProgress;
}

PanelOtaStatus panelOtaStatus() {
  return sStatus;
}

const char* panelOtaLastError() {
  return sError.c_str();
}

bool panelOtaBegin(size_t expectedSize, uint32_t expectedCrc32) {
  if (panelOtaIsActive()) {
    sError = "OTA already active";
    return false;
  }
  if (expectedSize == 0) {
    sError = "Invalid size";
    return false;
  }

  if (!Update.begin(expectedSize, U_FLASH, 0, HIGH)) {
    sError = Update.errorString();
    return false;
  }

  sStatus = PanelOtaStatus::InProgress;
  sError = "";
  sExpectedSize = expectedSize;
  sExpectedCrc = expectedCrc32;
  sWritten = 0;
  sRunningCrc = 0;
  sRebootPending = false;
  sRebootAtMs = 0;
  return true;
}

int panelOtaWrite(const uint8_t *data, size_t len) {
  if (!panelOtaIsActive()) {
    sError = "OTA not started";
    return -1;
  }
  if (!data || len == 0) {
    return 0;
  }

  size_t remain = (sExpectedSize > sWritten) ? (sExpectedSize - sWritten) : 0;
  if (len > remain) {
    len = remain;
  }

  size_t w = Update.write(const_cast<uint8_t *>(data), len);
  if (w != len) {
    sError = Update.errorString();
    sStatus = PanelOtaStatus::Failed;
    return -1;
  }

  sWritten += w;
  if (sExpectedCrc != 0) {
    sRunningCrc = crc32_update(sRunningCrc, data, w);
  }
  return static_cast<int>(w);
}

bool panelOtaEnd(bool rebootAfter) {
  if (!panelOtaIsActive()) {
    sError = "OTA not active";
    return false;
  }

  if (sWritten != sExpectedSize) {
    sError = "Size mismatch";
    Update.abort();
    sStatus = PanelOtaStatus::Failed;
    return false;
  }

  if (sExpectedCrc != 0 && sRunningCrc != sExpectedCrc) {
    sError = "CRC mismatch";
    Update.abort();
    sStatus = PanelOtaStatus::Failed;
    return false;
  }

  if (!Update.end(true)) {
    sError = Update.errorString();
    sStatus = PanelOtaStatus::Failed;
    Update.abort();
    return false;
  }

  sStatus = PanelOtaStatus::Success;
  if (rebootAfter) {
    sRebootPending = true;
    sRebootAtMs = millis() + 200;
  }
  return true;
}

void panelOtaAbort() {
  if (panelOtaIsActive()) {
    Update.abort();
  }
  resetState();
  sError = "OTA aborted";
}
