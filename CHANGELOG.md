## 2025-10-30

- Lengkapi handler UART: telemetri 10/1 Hz, setter NVS dengan ACK, sinkronisasi RTC (offset/rate-limit), dan streaming OTA base64 dengan guard OTA siap reboot.
- Tambahkan util RTC di modul sensor (ISO8601 dengan sufiks Z, get/set epoch) serta jadwal reboot aman pada OTA.
- Perbarui dokumentasi (`README.md`) dengan skema telemetri, command, kebijakan RTC, dan mapping GPIO final + contoh NVS lengkap.
- Integrasikan factory reset manual (Power+BOOT) dan via UART dengan log `factory_reset_executed`, buzzer ganda, serta reboot aman.
- Kembalikan dependensi OneWire dan DallasTemperature ke repositori upstream melalui `lib_deps` serta perbarui platform Espressif32 ke rilis terbaru.
- Sinkronkan versi platform dan library dengan rilis terbaru (espressif32 6.12.0, OneWire 2.3.8, DallasTemperature 4.0.5, ArduinoJson 7.4.2, Adafruit ADS1X15 2.6.0, SSD1306 2.5.15, GFX 1.12.3, U8g2 2.36.15, arduinoFFT 2.0.4).
- Rapikan dokumentasi `README.md` dengan daftar isi, tabel fitur, dan penjelasan factory reset/command sehingga lebih mudah diikuti panel & teknisi servis.

### File yang diubah
- README.md
- CHANGELOG.md
- include/comms.h
- include/main.h
- include/ui.h
- include/sensors.h
- src/sensors.cpp
- src/comms.cpp
- src/main.cpp
- src/ota.cpp
- src/ui.cpp
- platformio.ini
