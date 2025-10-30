#include "comms.h"
#include "config.h"
#include "state.h"
#include "power.h"
#include "sensors.h"
#include "buzzer.h"
#include "ota.h"
#include "main.h"

#include <ArduinoJson.h>
#include <mbedtls/base64.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

extern HardwareSerial espSerial;           // dideklarasi di main: HardwareSerial espSerial(2)
static HardwareSerial &linkSerial = espSerial;

// -------------------- RX line buffer --------------------
static String   rxLine;
static uint32_t lastRxBlink = 0;
static uint32_t lastTxBlink = 0;

// -------------------- Telemetry pacing ------------------
static uint32_t lastTelMs = 0;
static bool     otaReady = true;
static bool     forceTel = false;

// -------------------- Helpers ---------------------------
static inline uint32_t ms() { return millis(); }

static inline void ledRxPulse() { digitalWrite(LED_UART_PIN, HIGH); lastRxBlink = ms(); }
static inline void ledTxPulse() { digitalWrite(LED_UART_PIN, HIGH); lastTxBlink = ms(); }
static inline void ledActivityTick(uint32_t now) {
  if (now - lastRxBlink > 60 && now - lastTxBlink > 60) {
    digitalWrite(LED_UART_PIN, LOW);
  }
}

template <typename TDoc>
static void sendDoc(const TDoc &doc) {
  String out;
  serializeJson(doc, out);
  linkSerial.println(out);
  ledTxPulse();
}

static bool equalsIgnoreCase(const char *a, const char *b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower(*a) != tolower(*b)) return false;
    ++a; ++b;
  }
  return *a == *b;
}

static const char *fanModeToStr(FanMode m) {
  switch (m) {
    case FanMode::AUTO:     return "auto";
    case FanMode::CUSTOM:   return "custom";
    case FanMode::FAILSAFE: return "failsafe";
    default:                return "auto";
  }
}

static bool fanModeFromStr(const char *s, FanMode &out) {
  if (equalsIgnoreCase(s, "auto")) { out = FanMode::AUTO; return true; }
  if (equalsIgnoreCase(s, "custom")) { out = FanMode::CUSTOM; return true; }
  if (equalsIgnoreCase(s, "failsafe")) { out = FanMode::FAILSAFE; return true; }
  return false;
}

static bool variantIsNumber(const JsonVariant &v) {
  return v.is<int>() || v.is<unsigned int>() || v.is<long>() || v.is<unsigned long>() ||
         v.is<float>() || v.is<double>();
}

#define HANDLE_IF_PRESENT(key, handler)           \
  do {                                            \
    JsonVariant value = cmd[key];                 \
    if (!value.isNull()) {                        \
      handler(value);                             \
    }                                             \
  } while (0)

static void writeTimeISO(JsonObject obj) {
  char buf[24];
  if (!sensorsGetTimeISO(buf, sizeof(buf)) || strlen(buf) < 20) {
    snprintf(buf, sizeof(buf), "1970-01-01T00:00:00Z");
  }
  obj["time"] = buf;
}

static void writeNvsSnapshot(JsonObject root) {
  JsonObject nv = root["nvs"].to<JsonObject>();
  FanMode mode = stateGetFanMode();
  nv["fan_mode"]     = static_cast<uint8_t>(mode);
  nv["fan_mode_str"] = fanModeToStr(mode);
  nv["fan_duty"]     = stateGetFanCustomDuty();
  nv["spk_big"]      = stateSpeakerIsBig();
  nv["spk_pwr"]      = stateSpeakerPowerOn();
  nv["bt_en"]        = stateBtEnabled();
  nv["bt_autooff"]   = stateBtAutoOffMs();
  nv["smps_bypass"]  = stateSmpsBypass();
  nv["smps_cut"]     = stateSmpsCutoffV();
  nv["smps_rec"]     = stateSmpsRecoveryV();
}

static void writeErrors(JsonArray arr) {
  float v = getVoltageInstant();
  if (!stateSmpsBypass()) {
    if (v == 0.0f) {
      arr.add("NO_POWER");
    } else if (v < stateSmpsCutoffV()) {
      arr.add("LOW_VOLTAGE");
    }
  }
  if (isnan(getHeatsinkC())) {
    arr.add("SENSOR_FAIL");
  }
  if (powerSpkProtectFault()) {
    arr.add("SPK_PROTECT");
  }
}

static void writeAnalyzer(JsonObject data) {
  uint8_t bands[ANA_BANDS];
  analyzerGetBytes(bands, ANA_BANDS);
  JsonArray an = data["an"].to<JsonArray>();
  for (int i = 0; i < ANA_BANDS; ++i) {
    an.add((uint16_t)bands[i]);
  }
  uint8_t vu = 0;
  analyzerGetVu(vu);
  uint16_t vu1023 = (uint16_t)(((uint32_t)vu * 1023u + 127u) / 255u);
  data["vu"] = vu1023;
}

static void sendTelemetry() {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["ver"]  = "1";
  root["type"] = "telemetry";

  JsonObject data = root["data"].to<JsonObject>();
  writeTimeISO(data);
  data["fw_ver"]    = FW_VERSION;
  data["ota_ready"] = otaReady;

  data["smps_v"] = getVoltageInstant();
  data["heat_c"] = getHeatsinkC();

  JsonObject inputs = data["inputs"].to<JsonObject>();
  inputs["bt"]      = powerBtMode();
  inputs["speaker"] = powerGetSpeakerSelectBig() ? "big" : "small";

  JsonObject states = data["states"].to<JsonObject>();
  states["on"]      = powerIsOn();
  states["standby"] = powerIsStandby();

  JsonArray errs = data["errors"].to<JsonArray>();
  writeErrors(errs);

  writeAnalyzer(data);
  writeNvsSnapshot(data);

  sendDoc(root);
}

template <typename TValue>
static void sendAckOk(const char *key, const TValue &value) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"]    = "ack";
  root["ok"]      = true;
  root["changed"] = key;
  root["value"]   = value;
  sendDoc(root);
}

static void sendAckErr(const char *key, const char *reason) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"]  = "ack";
  root["ok"]    = false;
  root["error"] = reason ? reason : "invalid";
  if (key) {
    root["changed"] = key;
  }
  sendDoc(root);
}

static void sendLogInfoOffset(int32_t offset) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["ver"]         = "1";
  root["type"]        = "log";
  root["lvl"]         = "info";
  root["msg"]         = "rtc_synced";
  root["offset_sec"]  = offset;
  sendDoc(root);
}

static void sendLogWarnReason(const char *msg, const char *reason) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["ver"]    = "1";
  root["type"]   = "log";
  root["lvl"]    = "warn";
  root["msg"]    = msg;
  root["reason"] = reason;
  sendDoc(root);
}

static void sendLogErrorReason(const char *msg, const char *reason) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["ver"]    = "1";
  root["type"]   = "log";
  root["lvl"]    = "error";
  root["msg"]    = msg;
  root["reason"] = reason;
  sendDoc(root);
}

void commsLogFactoryReset(const char* src) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["ver"] = "1";
  root["type"] = "log";
  root["lvl"] = "info";
  root["msg"] = "factory_reset_executed";
  if (src && src[0] != '\0') {
    root["src"] = src;
  }
  sendDoc(root);
}

static void sendOtaEvent(const char *evt) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "ota";
  root["evt"]  = evt;
  sendDoc(root);
}

template <typename TValue>
static void sendOtaEvent(const char *evt, const char *field, const TValue &value) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"]  = "ota";
  root["evt"]   = evt;
  root[field]    = value;
  sendDoc(root);
}

static void sendOtaWriteOk(uint32_t seq) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "ota";
  root["evt"]  = "write_ok";
  root["seq"]  = seq;
  sendDoc(root);
}

static void sendOtaWriteErr(uint32_t seq, const char *err) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "ota";
  root["evt"]  = "write_err";
  root["seq"]  = seq;
  root["err"]  = err ? err : "error";
  sendDoc(root);
}

static void sendOtaError(const char *err) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "ota";
  root["evt"]  = "error";
  root["err"]  = err ? err : "unknown";
  sendDoc(root);
}

// -------------------- RTC helpers -----------------------
static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int)doe - 719468;
}

static bool parseIso8601ToEpoch(const char *iso, uint32_t &epochOut) {
  if (!iso) return false;
  int y = 0, m = 0, d = 0, hh = 0, mm = 0, ss = 0;
  if (sscanf(iso, "%4d-%2d-%2dT%2d:%2d:%2d", &y, &m, &d, &hh, &mm, &ss) != 6) return false;
  if (y < 2000 || m < 1 || m > 12 || d < 1 || d > 31 ||
      hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59) {
    return false;
  }
  int64_t days = daysFromCivil(y, (unsigned)m, (unsigned)d);
  int64_t secs = days * 86400 + hh * 3600 + mm * 60 + ss;
  if (secs < 0 || secs > 0xFFFFFFFFLL) return false;
  epochOut = (uint32_t)secs;
  return true;
}

static bool parseHex32(const char *hex, uint32_t &valueOut) {
  if (!hex) return false;
  uint32_t val = 0;
  const char *p = hex;
  if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
  int digits = 0;
  while (*p) {
    char c = *p++;
    uint8_t nibble = 0;
    if (c >= '0' && c <= '9') nibble = (uint8_t)(c - '0');
    else if (c >= 'a' && c <= 'f') nibble = (uint8_t)(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') nibble = (uint8_t)(c - 'A' + 10);
    else return false;
    val = (val << 4) | nibble;
    ++digits;
    if (digits > 8) return false;
  }
  if (digits == 0) return false;
  valueOut = val;
  return true;
}

static void handleRtcSync(uint32_t targetEpoch) {
  uint32_t currentEpoch = 0;
  if (!sensorsGetUnixTime(currentEpoch)) {
    sendLogErrorReason("rtc_sync_failed", "rtc_unavailable");
    return;
  }

  int32_t offset = (int32_t)((int64_t)targetEpoch - (int64_t)currentEpoch);
  int32_t absOffset = offset >= 0 ? offset : -offset;
  if (absOffset <= RTC_SYNC_MIN_OFFS_SEC) {
    sendLogWarnReason("rtc_sync_skipped", "offset_small");
    return;
  }

  const uint32_t minInterval = (uint32_t)RTC_SYNC_MIN_INTERVAL_H * 3600UL;
  uint32_t last = stateLastRtcSync();
  uint32_t ref = (targetEpoch > currentEpoch) ? targetEpoch : currentEpoch;
  if (last != 0 && (ref - last) < minInterval) {
    sendLogWarnReason("rtc_sync_skipped", "ratelimited");
    return;
  }

  if (!sensorsSetUnixTime(targetEpoch)) {
    sendLogErrorReason("rtc_sync_failed", "rtc_set_fail");
    return;
  }

  stateSetLastRtcSync(targetEpoch);
  sendLogInfoOffset(offset);
  forceTel = true;
}

// -------------------- Command handlers ------------------
static void handleCmdPower(JsonVariant v) {
  if (!v.is<bool>()) {
    sendAckErr("power", "invalid");
    return;
  }
  bool on = v.as<bool>();
  powerSetMainRelay(on);
  if (on) buzzerClick();
  sendAckOk("power", on);
  forceTel = true;
}

static void handleCmdBt(JsonVariant v) {
  if (!v.is<bool>()) {
    sendAckErr("bt", "invalid");
    return;
  }
  bool en = v.as<bool>();
  powerSetBtEnabled(en);
  sendAckOk("bt", en);
  forceTel = true;
}

static void handleCmdSpkSel(JsonVariant v) {
  if (!v.is<const char*>()) {
    sendAckErr("spk_sel", "invalid");
    return;
  }
  const char *s = v.as<const char*>();
  bool big;
  if (equalsIgnoreCase(s, "big")) {
    big = true;
  } else if (equalsIgnoreCase(s, "small")) {
    big = false;
  } else {
    sendAckErr("spk_sel", "invalid");
    return;
  }
  powerSetSpeakerSelect(big);
  sendAckOk("spk_sel", big ? "big" : "small");
  forceTel = true;
}

static void handleCmdSpkPwr(JsonVariant v) {
  if (!v.is<bool>()) {
    sendAckErr("spk_pwr", "invalid");
    return;
  }
  bool on = v.as<bool>();
  powerSetSpeakerPower(on);
  sendAckOk("spk_pwr", on);
  forceTel = true;
}

static void handleCmdSmpsBypass(JsonVariant v) {
  if (!v.is<bool>()) {
    sendAckErr("smps_bypass", "invalid");
    return;
  }
  bool en = v.as<bool>();
  stateSetSmpsBypass(en);
  sendAckOk("smps_bypass", en);
  forceTel = true;
}

static void handleCmdSmpsCut(JsonVariant v) {
  if (!variantIsNumber(v)) {
    sendAckErr("smps_cut", "invalid");
    return;
  }
  float cut = v.as<float>();
  if (cut < 30.0f || cut > 70.0f || cut >= stateSmpsRecoveryV()) {
    sendAckErr("smps_cut", "range");
    return;
  }
  stateSetSmpsCutoffV(cut);
  sendAckOk("smps_cut", cut);
  forceTel = true;
}

static void handleCmdSmpsRec(JsonVariant v) {
  if (!variantIsNumber(v)) {
    sendAckErr("smps_rec", "invalid");
    return;
  }
  float rec = v.as<float>();
  if (rec < 30.0f || rec > 80.0f || rec <= stateSmpsCutoffV()) {
    sendAckErr("smps_rec", "range");
    return;
  }
  stateSetSmpsRecoveryV(rec);
  sendAckOk("smps_rec", rec);
  forceTel = true;
}

static void handleCmdBtAutoOff(JsonVariant v) {
  if (!variantIsNumber(v)) {
    sendAckErr("bt_autooff", "invalid");
    return;
  }
  double valD = v.as<double>();
  if (valD < 0.0 || valD > 3600000.0) {
    sendAckErr("bt_autooff", "range");
    return;
  }
  uint32_t val = (uint32_t)(valD + 0.5);
  stateSetBtAutoOffMs(val);
  sendAckOk("bt_autooff", val);
  forceTel = true;
}

static void handleCmdFanMode(JsonVariant v) {
  if (!v.is<const char*>()) {
    sendAckErr("fan_mode", "invalid");
    return;
  }
  FanMode mode;
  if (!fanModeFromStr(v.as<const char*>(), mode)) {
    sendAckErr("fan_mode", "invalid");
    return;
  }
  stateSetFanMode(mode);
  sendAckOk("fan_mode", fanModeToStr(mode));
  forceTel = true;
}

static void handleCmdFanDuty(JsonVariant v) {
  if (!variantIsNumber(v)) {
    sendAckErr("fan_duty", "invalid");
    return;
  }
  int duty = (int)std::lround(v.as<double>());
  if (duty < 0 || duty > 1023) {
    sendAckErr("fan_duty", "range");
    return;
  }
  stateSetFanCustomDuty((uint16_t)duty);
  sendAckOk("fan_duty", duty);
  forceTel = true;
}

static void handleCmdRtcSet(JsonVariant v) {
  if (!v.is<const char*>()) {
    sendAckErr("rtc_set", "invalid");
    return;
  }
  uint32_t epoch = 0;
  if (!parseIso8601ToEpoch(v.as<const char*>(), epoch)) {
    sendAckErr("rtc_set", "invalid");
    return;
  }
  handleRtcSync(epoch);
}

static void handleCmdRtcSetEpoch(JsonVariant v) {
  if (!variantIsNumber(v)) {
    sendAckErr("rtc_set_epoch", "invalid");
    return;
  }
  uint32_t epoch = v.as<uint32_t>();
  handleRtcSync(epoch);
}

static void handleCmdBuzz(JsonVariant v) {
  if (!v.is<JsonObject>()) {
    sendAckErr("buzz", "invalid");
    return;
  }
  JsonObject o = v.as<JsonObject>();
  uint32_t f = o["f"] | BUZZER_PWM_FREQ;
  uint16_t d = o["d"] | BUZZER_DUTY_CLICK;
  uint16_t msDur = o["ms"] | BUZZER_CLICK_MS;
  buzzerCustom(f, d, msDur);
  sendAckOk("buzz", true);
}

static void handleCmdNvsReset(JsonVariant v) {
  if (!v.is<bool>() || !v.as<bool>()) {
    sendAckErr("nvs_reset", "invalid");
    return;
  }
  stateFactoryReset();
  powerSetSpeakerSelect(stateSpeakerIsBig());
  powerSetSpeakerPower(stateSpeakerPowerOn());
  powerSetBtEnabled(stateBtEnabled());
  sendAckOk("nvs_reset", true);
  forceTel = true;
}

static void handleCmdFactoryReset(JsonVariant v) {
  if (!v.is<bool>() || !v.as<bool>()) {
    sendAckErr("factory_reset", "invalid");
    return;
  }
  if (powerIsOn()) {
    sendAckErr("factory_reset", "system_active");
    return;
  }
  sendAckOk("factory_reset", true);
  forceTel = true;
  appPerformFactoryReset("FACTORY RESET (UART)", "uart");
}

static void handleCmdOtaBegin(JsonVariant v) {
  if (!v.is<JsonObject>()) {
    sendOtaEvent("begin_err", "err", "invalid");
    sendOtaError("invalid_begin_payload");
    return;
  }
  JsonObject o = v.as<JsonObject>();
  size_t size = o["size"] | 0;
  const char *crcHex = o["crc32"] | nullptr;
  uint32_t crc = 0;
  if (crcHex && crcHex[0] != '\0') {
    if (!parseHex32(crcHex, crc)) {
      sendOtaEvent("begin_err", "err", "crc_invalid");
      sendOtaError("crc_invalid");
      return;
    }
  }
  if (!otaBegin(size, crc)) {
    const char *err = otaLastError();
    sendOtaEvent("begin_err", "err", err);
    sendOtaError(err);
    return;
  }
  powerSetOtaActive(true);
  commsSetOtaReady(false);
  sendOtaEvent("begin_ok");
  forceTel = true;
}

static void handleCmdOtaWrite(JsonVariant v) {
  if (!v.is<JsonObject>()) {
    sendOtaEvent("write_err", "err", "invalid");
    sendOtaError("invalid_write_payload");
    return;
  }
  JsonObject o = v.as<JsonObject>();
  uint32_t seq = o["seq"] | 0;
  const char *dataB64 = o["data_b64"] | nullptr;
  if (!dataB64) {
    sendOtaWriteErr(seq, "invalid_data");
    sendOtaError("invalid_data");
    return;
  }
  size_t inLen = strlen(dataB64);
  std::vector<uint8_t> decoded((inLen * 3) / 4 + 4);
  size_t outLen = 0;
  int rc = mbedtls_base64_decode(decoded.data(), decoded.size(), &outLen,
                                 reinterpret_cast<const unsigned char*>(dataB64), inLen);
  if (rc != 0) {
    sendOtaWriteErr(seq, "b64_decode");
    sendOtaError("b64_decode");
    return;
  }
  int wrote = otaWrite(decoded.data(), outLen);
  if (wrote < 0) {
    const char *err = otaLastError();
    sendOtaWriteErr(seq, err);
    sendOtaError(err);
    return;
  }
  sendOtaWriteOk(seq);
  otaYieldOnce();
}

static void handleCmdOtaEnd(JsonVariant v) {
  if (!v.is<JsonObject>()) {
    sendOtaEvent("end_err", "err", "invalid");
    sendOtaError("invalid_end_payload");
    return;
  }
  JsonObject o = v.as<JsonObject>();
  bool reboot = o["reboot"] | false;
  if (!otaEnd(reboot)) {
    const char *err = otaLastError();
    sendOtaEvent("end_err", "err", err);
    sendOtaError(err);
    return;
  }
  if (!reboot) {
    powerSetOtaActive(false);
    commsSetOtaReady(true);
  }
  sendOtaEvent("end_ok", "rebooting", reboot);
  forceTel = true;
}

static void handleCmdOtaAbort(JsonVariant v) {
  bool doAbort = v.is<bool>() ? v.as<bool>() : true;
  if (!doAbort) {
    sendOtaEvent("abort_ok");
    return;
  }
  otaAbort();
  sendOtaEvent("abort_ok");
  forceTel = true;
}

// -------------------- Dispatch --------------------------
static void handleJsonLine(const String &line) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err) return;

  const char *type = doc["type"] | "";
  if (strcmp(type, "cmd") != 0 && strcmp(type, "command") != 0) return;

  JsonObject root = doc.as<JsonObject>();
  JsonObject cmd = root["cmd"];
  if (cmd.isNull()) return;

  HANDLE_IF_PRESENT("power",         handleCmdPower);
  HANDLE_IF_PRESENT("bt",            handleCmdBt);
  HANDLE_IF_PRESENT("spk_sel",       handleCmdSpkSel);
  HANDLE_IF_PRESENT("spk_pwr",       handleCmdSpkPwr);
  HANDLE_IF_PRESENT("smps_bypass",   handleCmdSmpsBypass);
  HANDLE_IF_PRESENT("smps_cut",      handleCmdSmpsCut);
  HANDLE_IF_PRESENT("smps_rec",      handleCmdSmpsRec);
  HANDLE_IF_PRESENT("bt_autooff",    handleCmdBtAutoOff);
  HANDLE_IF_PRESENT("fan_mode",      handleCmdFanMode);
  HANDLE_IF_PRESENT("fan_duty",      handleCmdFanDuty);
  HANDLE_IF_PRESENT("rtc_set",       handleCmdRtcSet);
  HANDLE_IF_PRESENT("rtc_set_epoch", handleCmdRtcSetEpoch);
  HANDLE_IF_PRESENT("ota_begin",     handleCmdOtaBegin);
  HANDLE_IF_PRESENT("ota_write",     handleCmdOtaWrite);
  HANDLE_IF_PRESENT("ota_end",       handleCmdOtaEnd);
  HANDLE_IF_PRESENT("ota_abort",     handleCmdOtaAbort);
  HANDLE_IF_PRESENT("buzz",          handleCmdBuzz);
  HANDLE_IF_PRESENT("nvs_reset",     handleCmdNvsReset);
  HANDLE_IF_PRESENT("factory_reset", handleCmdFactoryReset);
}

#undef HANDLE_IF_PRESENT

// -------------------- PUBLIC API ------------------------
void commsInit() {
  pinMode(LED_UART_PIN, OUTPUT);
  digitalWrite(LED_UART_PIN, LOW);

  linkSerial.begin(SERIAL_BAUD_LINK, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
  rxLine.reserve(4096);
  lastTelMs = 0;
  otaReady = true;
  forceTel = true;
}

void commsTick(uint32_t now, bool sqwTick) {
  ledActivityTick(now);

  while (linkSerial.available()) {
    int c = linkSerial.read();
    if (c < 0) break;
    ledRxPulse();

    if (c == '\n' || c == '\r') {
      if (rxLine.length() > 0) {
        handleJsonLine(rxLine);
        rxLine = "";
      }
    } else {
      if (rxLine.length() < 4000) {
        rxLine += (char)c;
      } else {
        rxLine = ""; // buang baris yang terlalu panjang
      }
    }
  }

  uint16_t hzActive   = TELEMETRY_HZ_ACTIVE;
  uint16_t hzStandby  = TELEMETRY_HZ_STANDBY;
  uint32_t intervalActive  = (hzActive  > 0) ? (1000UL / hzActive)  : 0;
  uint32_t intervalStandby = (hzStandby > 0) ? (1000UL / hzStandby) : 0;

  bool shouldSend = forceTel;
  if (!shouldSend) {
    if (powerIsOn()) {
      if (intervalActive == 0 || now - lastTelMs >= intervalActive) {
        shouldSend = true;
      }
    } else {
      if (sqwTick) {
        shouldSend = true;
      } else {
        uint32_t fallback = (intervalStandby == 0) ? 1000UL : intervalStandby;
        if (now - lastTelMs >= fallback) {
          shouldSend = true;
        }
      }
    }
  }

  if (shouldSend) {
    sendTelemetry();
    lastTelMs = now;
    forceTel = false;
  }
}

void commsForceTelemetry() { forceTel = true; }

void commsSetOtaReady(bool ready) {
  if (otaReady != ready) {
    otaReady = ready;
    forceTel = true;  // kabarkan status OTA segera ke panel
  }
}

void commsLog(const char* level, const char* msg) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["ver"]  = "1";
  root["type"] = "log";
  root["lvl"]  = level ? level : "info";
  root["msg"]  = msg ? msg : "";
  sendDoc(root);
}
