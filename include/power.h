#pragma once
#include <Arduino.h>

// Inisialisasi GPIO (relay, fan PWM, BT, selector/power speaker, monitor, dll.)
void powerInit();

// Tick rutin (panggil di loop)
void powerTick(uint32_t now);

// Relay utama (aktif HIGH/LOW sesuai config)
void powerSetMainRelay(bool on);
bool powerMainRelay();

// Speaker: selector (BIG/SMALL) & power (supply speaker protector)
void powerSetSpeakerSelect(bool big);
bool powerGetSpeakerSelectBig();

void powerSetSpeakerPower(bool on);
bool powerGetSpeakerPower();

// Bluetooth: enable/disable modul dan status mode (BT vs AUX dari LED status)
void powerSetBtEnabled(bool en);
bool powerBtEnabled();
bool powerBtMode();  // true=BT mode (LED status LOW), false=AUX

// OTA guard (true → auto-power berbasis PC detect diabaikan)
void powerSetOtaActive(bool on);

// Fault monitor speaker protector LED
bool powerSpkProtectFault();

// Input mode string (untuk telemetri/UI ringkas)
const char* powerInputModeStr();