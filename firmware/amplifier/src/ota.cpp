#include "ota.h"
#include "config.h"
#include "comms.h"
#include "power.h"

#include <Update.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>

// ------------------- State -------------------
static OtaStatus sStatus = OtaStatus::Idle;
static String    sErr;
static size_t    sExpectedSize = 0;
static uint32_t  sExpectedCrc  = 0;
static size_t    sWritten      = 0;
static uint32_t  sCrcRunning   = 0;
static bool      sRebootPending = false;
static uint32_t  sRebootAtMs    = 0;

// CRC32 tabel (polynomial 0xEDB88320)
static uint32_t crc32_update(uint32_t crc, const uint8_t *buf, size_t len) {
  static uint32_t table[256];
  static bool init = false;
  if (!init) {
    for (uint32_t i=0; i<256; ++i) {
      uint32_t c = i;
      for (int j=0; j<8; ++j) {
        if (c & 1) c = 0xEDB88320UL ^ (c >> 1);
        else       c = c >> 1;
      }
      table[i] = c;
    }
    init = true;
  }
  crc = ~crc;
  for (size_t i=0; i<len; ++i) {
    crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
  }
  return ~crc;
}

static inline void setError(const char* msg) {
  sErr = msg ? msg : "OTA error";
}

void otaInit() {
  sStatus = OtaStatus::Idle;
  sErr = "";
  sExpectedSize = 0;
  sExpectedCrc = 0;
  sWritten = 0;
  sCrcRunning = 0;
  sRebootPending = false;
  sRebootAtMs = 0;
  commsSetOtaReady(true);
  powerSetOtaActive(false);
}

void otaTick(uint32_t now) {
  if (sRebootPending && now >= sRebootAtMs) {
    sRebootPending = false;
    delay(50);
    ESP.restart();
  }
}

OtaStatus otaStatus() { return sStatus; }
const char* otaLastError() { return sErr.c_str(); }

bool otaBegin(size_t expectedSize, uint32_t expectedCrc32) {
  if (sStatus == OtaStatus::InProgress) {
    setError("OTA already in progress");
    return false;
  }
  if (expectedSize == 0 || expectedSize > OTA_MAX_BIN_SIZE) {
    setError("Invalid size");
    return false;
  }

  // Siapkan partisi OTA berikutnya
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (!next) {
    setError("No OTA partition");
    return false;
  }

  // Mulai Update
  if (!Update.begin(expectedSize, U_FLASH, 0, HIGH)) {
    setError(Update.errorString());
    return false;
  }

  // Set guard/telemetry
  commsSetOtaReady(false);
  powerSetOtaActive(true);

  sExpectedSize = expectedSize;
  sExpectedCrc  = expectedCrc32;   // 0 = skip check
  sWritten      = 0;
  sCrcRunning   = 0;
  sStatus       = OtaStatus::InProgress;
  sErr          = "";
  sRebootPending = false;
  sRebootAtMs    = 0;

  return true;
}

int otaWrite(const uint8_t* data, size_t len) {
  if (sStatus != OtaStatus::InProgress) {
    setError("OTA not started");
    return -1;
  }
  if (!data || len == 0) return 0;

  // Cegah overflow ukuran
  size_t remain = (sExpectedSize > sWritten) ? (sExpectedSize - sWritten) : 0;
  if (len > remain) len = remain;

  size_t w = Update.write(const_cast<uint8_t*>(data), len);
  if (w != len) {
    setError(Update.errorString());
    sStatus = OtaStatus::Failed;
    return -1;
  }
  sWritten += w;
  if (sExpectedCrc) {
    sCrcRunning = crc32_update(sCrcRunning, data, w);
  }
  return (int)w;
}

bool otaEnd(bool doReboot) {
  if (sStatus != OtaStatus::InProgress) {
    setError("OTA not in progress");
    return false;
  }

  // Ukuran harus pas
  if (sWritten != sExpectedSize) {
    setError("Size mismatch");
    sStatus = OtaStatus::Failed;
    Update.abort();
    commsSetOtaReady(true);
    powerSetOtaActive(false);
    return false;
  }

  // CRC jika diminta
  if (sExpectedCrc && sCrcRunning != sExpectedCrc) {
    setError("CRC mismatch");
    sStatus = OtaStatus::Failed;
    Update.abort();
    commsSetOtaReady(true);
    powerSetOtaActive(false);
    return false;
  }

  // End & set boot partition
  if (!Update.end(true)) {
    setError(Update.errorString());
    sStatus = OtaStatus::Failed;
    Update.abort();
    commsSetOtaReady(true);
    powerSetOtaActive(false);
    return false;
  }

  sStatus = OtaStatus::Success;
  sErr = "";

  // Kembalikan flag & reboot jika diminta
  if (!doReboot) {
    commsSetOtaReady(true);
    powerSetOtaActive(false);
  }

  if (doReboot) {
    sRebootPending = true;
    sRebootAtMs = millis() + 200;
  }
  return true;
}

void otaAbort() {
  if (sStatus == OtaStatus::InProgress) {
    Update.abort();
  }
  sStatus = OtaStatus::Idle;
  sErr = "OTA aborted";
  sExpectedSize = 0;
  sExpectedCrc = 0;
  sWritten = 0;
  sCrcRunning = 0;
  sRebootPending = false;
  sRebootAtMs = 0;

  commsSetOtaReady(true);
  powerSetOtaActive(false);
}

void otaYieldOnce() {
  // Tempat untuk yield kalau transfer panjang
  delay(0);
}