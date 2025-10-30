#pragma once
#include <Arduino.h>

// Init LEDC untuk buzzer (mengambil konstanta dari config.h)
void buzzerInit();

// Panggil rutin di loop
void buzzerTick(uint32_t now);

// Pola standar (menggunakan duty & durasi dari config.h)
void buzzerClick();                 // klik sangat singkat (tiap tap UI)
void buzzerWarning();               // nada peringatan pendek
void buzzerError();                 // nada error panjang, bisa repeat
void buzzerErrorPattern();          // alias pattern error untuk kompatibilitas

// Penghenti paksa (mematikan output buzzer)
void buzzerStop();

// Nada kustom dari panel (freq Hz, duty 0..1023, durasi ms).
// Duty diabaikan jika > resolusi; akan diklip ke 0..1023.
void buzzerCustom(uint32_t freqHz, uint16_t duty, uint16_t ms);

// Status
bool buzzerIsActive();
