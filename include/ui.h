#pragma once
#include <Arduino.h>

// Init OLED & buffer, panggil sekali dari setup()
void uiInit();

// Tick refresh UI; panggil rutin dari loop()
void uiTick(uint32_t now);

// --------- Layar/Scene ---------
void uiShowSplash(const char* title);                // splash awal
void uiBootLogLine(const char* label, bool ok);      // baris boot log: OK/FAIL
void uiShowError(const char* msg);                   // layar error penuh
void uiShowWarning(const char* msg);                 // layar warning/info
void uiShowStandby();                                // paksa ke layar standby
void uiShowBoot(uint32_t holdMs);                    // splash boot bawaan
void uiShowFactoryReset(const char* subtitle, uint32_t holdMs); // layar factory reset

// Update jam "HH:MM:SS" untuk OLED (di-set dari RTC/telemetri)
void uiSetClock(const char* hhmmss);

// Abstraksi kecil untuk notifikasi status input (AUX/BT) dan speaker
void uiSetInputStatus(bool btMode, bool speakerBig);
