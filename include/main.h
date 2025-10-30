#pragma once
#include <Arduino.h>

// Titik masuk high-level app lifecycle
void appInit();      // panggil dari setup()
void appTick();      // panggil dari loop()

// Jalankan factory reset terkelola (OLED + buzzer + log + reboot)
void appPerformFactoryReset(const char* subtitle, const char* src);
