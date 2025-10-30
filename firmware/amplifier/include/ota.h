#pragma once
#include <Arduino.h>

// Status ringkas OTA
enum class OtaStatus : uint8_t {
  Idle = 0,
  InProgress,
  Success,
  Failed
};

// Init/loop helper untuk state mesin OTA via UART
void otaInit();
void otaTick(uint32_t now);

// Mulai sesi OTA.
// - expectedSize : ukuran file .bin (wajib, >0, <= OTA_MAX_BIN_SIZE)
// - expectedCrc32: CRC32 file penuh (0 = lewati cek CRC, selain 0 = wajib cocok)
// Return: true jika sesi berhasil disiapkan, false bila gagal (lihat otaLastError()).
bool otaBegin(size_t expectedSize, uint32_t expectedCrc32);

// Tulis blok data biner ke partisi OTA aktif.
// Return: jumlah byte yang benar-benar ditulis; -1 jika error (cek otaLastError()).
int  otaWrite(const uint8_t* data, size_t len);

// Akhiri sesi OTA.
// - doReboot: jika true dan sukses → setBootPartition + ESP.restart()
// Return: true jika sukses komplit; false jika gagal (cek otaLastError()).
bool otaEnd(bool doReboot);

// Batalkan sesi OTA yang sedang berjalan (rollback & clear flag).
void otaAbort();

// Status & diagnostics
OtaStatus otaStatus();
const char* otaLastError();  // pesan terakhir (ringkas, untuk log/telemetry)

// (Opsional) utility flush jika transport punya batas pacing
void otaYieldOnce();
