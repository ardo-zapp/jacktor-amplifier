#include "ui.h"
#include "config.h"
#include "state.h"
#include "power.h"
#include "sensors.h"

#include <U8g2lib.h>
#include <cstring>

// ================= OLED DRIVER =================
/*
 * SSD1306 128x64 I2C; gunakan default I2C (Wire) dari board.
 * Jika modulmu berbeda, ganti konstruktor di sini.
 */
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// ================= UI STATE =================
enum class UiScene : uint8_t {
  SPLASH,
  BOOTLOG,
  STANDBY,
  RUN,
  ERROR,
  WARN
};

static UiScene gScene = UiScene::SPLASH;
static char    gClock[9] = "00:00:00";

static bool    gBtMode = false;     // true=BT, false=AUX
static bool    gSpkBig = SPK_DEFAULT_BIG;

// Bootlog ringkas
static uint8_t gBootRows = 0;
static const uint8_t MAX_BOOT_ROWS = 6; // 6 baris muat di 128x64

// Pace refresh
static uint32_t lastDrawMs = 0;

// ================= SMALL HELPERS =================
static inline void drawHeader(const char* title) {
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 10, title);
  u8g2.drawHLine(0, 12, 128);
}

static inline void drawKeyVal(int y, const char* k, const char* v) {
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, y, k);
  u8g2.drawStr(70, y, v);
}

static void drawStandbyScreen() {
  u8g2.clearBuffer();
  drawHeader("STANDBY");

  // Jam besar
  u8g2.setFont(u8g2_font_logisoso22_tf);
  u8g2.drawStr(6, 45, gClock);

  // Tegangan kecil di bawah
  char vbuf[16];
  float v = getVoltageInstant();
  snprintf(vbuf, sizeof(vbuf), "V: %.1f", v);
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 62, vbuf);

  if (powerSpkProtectFault()) {
    u8g2.drawStr(70, 62, "SPK PROTECT FAIL");
  }

  u8g2.sendBuffer();
}

static void drawRunScreen() {
  u8g2.clearBuffer();
  drawHeader("AMPLIFIER");

  // Baris status input/speaker
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 24, gBtMode ? "IN: BT" : "IN: AUX");
  u8g2.drawStr(64,24, gSpkBig ? "SPK: BIG" : "SPK: SMALL");

  // Tegangan & Suhu
  char vbuf[16], tbuf[16];
  float v = getVoltageInstant();
  float t = getHeatsinkC();
  snprintf(vbuf, sizeof(vbuf), "V: %.1f", v);
  if (isnan(t)) snprintf(tbuf, sizeof(tbuf), "T: --.-C");
  else          snprintf(tbuf, sizeof(tbuf), "T: %.1fC", t);

  u8g2.drawStr(0, 38, vbuf);
  u8g2.drawStr(64,38, tbuf);

  // VU meter (mono; tinggi 18 px, lebar 120 px)
  uint8_t vu = 0; analyzerGetVu(vu);           // 0..255
  int vuW = map(vu, 0, 255, 0, 120);
  int vuX = 4, vuY = 60, vuH = 12;
  u8g2.drawFrame(vuX, vuY - vuH, 120, vuH);    // frame
  if (vuW > 0) u8g2.drawBox(vuX+1, vuY - vuH + 1, vuW, vuH - 2);

  // Jam kecil di pojok kanan bawah
  u8g2.drawStr(92, 62, gClock);

  if (powerSpkProtectFault()) {
    u8g2.drawStr(0, 52, "SPK PROTECT FAIL");
  }

  u8g2.sendBuffer();
}

static void drawSplash(const char* title) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 10, title ? title : "JACKTOR AMP");
  u8g2.drawHLine(0, 12, 128);
  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.drawStr(10, 40, "Booting...");
  u8g2.sendBuffer();
}

static void drawBootLogLine(const char* label, bool ok) {
  if (gBootRows >= MAX_BOOT_ROWS) return;
  int y = 24 + 10 * gBootRows;
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, y, label ? label : "?");
  u8g2.drawStr(110, y, ok ? "OK" : "FAIL");
  gBootRows++;
}

static void drawError(const char* msg) {
  u8g2.clearBuffer();
  drawHeader("ERROR");
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 28, msg ? msg : "Unknown error");
  u8g2.sendBuffer();
}

static void drawWarning(const char* msg) {
  u8g2.clearBuffer();
  drawHeader("NOTICE");
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 28, msg ? msg : "Notice");
  u8g2.sendBuffer();
}

// ================= PUBLIC API =================
void uiInit() {
  u8g2.begin();
  u8g2.setPowerSave(0);
  gScene = powerIsOn() ? UiScene::RUN : UiScene::STANDBY;
  lastDrawMs = 0;
}

void uiShowBoot(uint32_t holdMs) {
  gScene = UiScene::SPLASH;
  drawSplash(FW_NAME);
  if (holdMs > 0) {
    delay(holdMs);
  }
}

void uiShowFactoryReset(const char* subtitle, uint32_t holdMs) {
  gScene = UiScene::WARN;
  u8g2.clearBuffer();
  drawHeader("FACTORY RESET");
  u8g2.setFont(u8g2_font_6x12_tf);
  const char *line = (subtitle && subtitle[0]) ? subtitle : "Menghapus NVS...";
  u8g2.drawStr(0, 32, line);
  u8g2.sendBuffer();
  if (holdMs > 0) {
    delay(holdMs);
  }
}

void uiTick(uint32_t now) {
  // 30 FPS max agar hemat
  if (now - lastDrawMs < 33) return;
  lastDrawMs = now;

  // Transisi scene berdasar power state:
  if (powerIsStandby() && gScene != UiScene::STANDBY) {
    gScene = UiScene::STANDBY;
  } else if (powerIsOn() && gScene == UiScene::STANDBY) {
    gScene = UiScene::RUN;
  }

  switch (gScene) {
    case UiScene::SPLASH:   /* keep */ break; // hanya by call eksplisit
    case UiScene::BOOTLOG:  u8g2.sendBuffer(); break; // baris ditulis per event
    case UiScene::STANDBY:  drawStandbyScreen(); break;
    case UiScene::RUN:      drawRunScreen(); break;
    case UiScene::ERROR:    /* static */ break;
    case UiScene::WARN:     /* static */ break;
  }
}

void uiShowSplash(const char* title) {
  gScene = UiScene::SPLASH;
  drawSplash(title);
}

void uiBootLogLine(const char* label, bool ok) {
  if (gScene != UiScene::BOOTLOG) {
    gScene = UiScene::BOOTLOG;
    u8g2.clearBuffer();
    drawHeader("BOOT LOG");
  }
  drawBootLogLine(label, ok);
  u8g2.sendBuffer();
}

void uiShowError(const char* msg) {
  gScene = UiScene::ERROR;
  drawError(msg);
}

void uiShowWarning(const char* msg) {
  gScene = UiScene::WARN;
  drawWarning(msg);
}

void uiShowStandby() {
  gScene = UiScene::STANDBY;
  drawStandbyScreen();
}

void uiSetClock(const char* hhmmss) {
  if (!hhmmss) return;
  strncpy(gClock, hhmmss, sizeof(gClock)-1);
  gClock[sizeof(gClock)-1] = '\0';
}

void uiSetInputStatus(bool btMode, bool speakerBig) {
  gBtMode = btMode;
  gSpkBig = speakerBig;
}