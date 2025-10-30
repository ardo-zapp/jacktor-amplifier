# Jacktor Panel Bridge Firmware (WIP)

_Dokumen ini menampung spesifikasi awal untuk firmware panel/bridge ESP32. Implementasi firmware masih dalam tahap perencanaan._

## GPIO Mapping Final

| GPIO | Arah | Fungsi | Level/Timing |
|------|------|--------|--------------|
| 17   | TX   | UART2 TX → Amplifier | — |
| 16   | RX   | UART2 RX ← Amplifier | — |
| 23   | OUT  | EN Amplifier (reset line) | LOW aktif |
| 27   | OUT  | GPIO0 Amplifier (flash mode) | LOW aktif |
| 32   | OUT  | Trigger tombol Power Android | LOW 1 s saat boot amplifier |
| 13   | OUT  | USB OTG ID Control | LOW = ID→GND |
| 34   | IN   | VBUS Sense (opto/ADC) | HIGH = 5V hadir |
| 25   | OUT  | LED Merah (status) | — |
| 26   | OUT  | LED Hijau (status) | — |
| 21   | I²C  | SDA (opsional RTC/expander) | — |
| 22   | I²C  | SCL (opsional RTC/expander) | — |
| 0    | IN   | Tombol BOOT Panel | — |
| EN   | IN   | Reset Panel | — |
| 33   | IN   | Cadangan input-only | — |
| 35   | IN   | Cadangan input | — |
| 36   | IN   | Cadangan input | — |
| 39   | IN   | Cadangan input | — |

## Roadmap Singkat

1. Otomasi trigger tombol Power Android (pulse 1 detik di awal boot amplifier).
2. Mekanisme OTG ID toggle dengan cooldown 15 menit & batas 10 pulse.
3. Bridge UART line-based antara panel dan amplifier (termasuk mode flash amplifier).
4. Forwarding komando time-sync (RTC) dan telemetry pass-through.

Dokumentasi lengkap, PlatformIO project, serta diagram state akan ditambahkan bersama implementasi firmware.
