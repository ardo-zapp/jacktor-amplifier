#pragma once
#include <Arduino.h>
#include "config.h"

// ---- Voltage & Temperature ----
void  sensorsInit();
void  sensorsTick(uint32_t now);

float getVoltageInstant();   // Volt (ADS1115, tanpa smoothing)
float getHeatsinkC();        // °C (DS18B20) atau NAN jika invalid
float sensorsGetRtcTempC();  // °C RTC internal (DS3231) atau NAN

// ---- Analyzer (FFT 16-band) ----
void  analyzerGetBytes(uint8_t* out, size_t n);  // 0..255 per band
void  analyzerGetVu(uint8_t& outVu);             // 0..255 mono VU
void  sensorsSetAnalyzerEnabled(bool en);        // matikan saat standby

// ---- RTC/SQW utils ----
bool  sensorsGetTimeISO(char* out, size_t n);    // "YYYY-MM-DDTHH:MM:SSZ"
bool  sensorsSqwConsumeTick();                   // true jika ada pulse 1 Hz
bool  sensorsGetUnixTime(uint32_t& epochOut);    // epoch detik (UTC)
bool  sensorsSetUnixTime(uint32_t epoch);        // set RTC ke epoch UTC
