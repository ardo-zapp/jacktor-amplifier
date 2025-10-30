#pragma once
#include <Arduino.h>

// Inisialisasi link UART2 ke panel
void commsInit();

// Dipanggil rutin di loop (sqwTick=true jika menerima pulse 1 Hz RTC)
void commsTick(uint32_t now, bool sqwTick);

// Minta kirim telemetri segera (mis. setelah aksi penting)
void commsForceTelemetry();

// Notifikasi OTA guard (agar panel tahu amplifier sedang OTA)
void commsSetOtaReady(bool ready);

// Kirim log singkat (opsional)
void commsLog(const char* level, const char* msg);

// Log khusus factory reset (sertakan sumber)
void commsLogFactoryReset(const char* src);
