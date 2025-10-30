#include <Arduino.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <vector>

#include "config.h"
#include "ota_panel.h"

enum OtgState { IDLE, PROBE, WAIT_VBUS, WAIT_HANDSHAKE, HOST_ACTIVE, BACKOFF, COOLDOWN };
enum LedPattern { LED_PATTERN_OFF, LED_PATTERN_SOLID, LED_PATTERN_BLINK_SLOW, LED_PATTERN_BLINK_FAST };

struct LedChannel {
  LedPattern pattern;
  bool outputState;
  uint32_t patternStart;
};

static OtgState otgState = IDLE;
static uint32_t stateMs = 0;
static uint32_t lastTick = 0;
static uint32_t probeStartMs = 0;
static size_t backoffIdx = 0;
static uint32_t pulseCount = 0;
static bool hostActive = false;
static bool vbusValid = false;
static bool vbusRawPrev = false;
static uint32_t vbusHighStartMs = 0;
static uint32_t vbusLowStartMs = 0;
static uint32_t lastVbusHighMs = 0;
static uint32_t vbusDropStartMs = 0;
static uint32_t currentBackoffMs = 0;
static uint32_t lastPowerWakeMs = 0;
static uint8_t powerWakeCount = 0;
static bool otgPulseActive = false;
static bool powerPulseActive = false;
static bool powerGraceActive = false;
static uint32_t powerPulseStartMs = 0;
static uint32_t powerGraceStartMs = 0;
static uint32_t lastHelloMs = 0;

static String hostRxBuffer;
static String ampRxBuffer;

static LedChannel redLed   = {LED_PATTERN_SOLID, true, 0};
static LedChannel greenLed = {LED_PATTERN_OFF, false, 0};

static bool panelOtaLatched = false;
static bool ampOtaActive = false;
static uint32_t panelOtaCliSeq = 0;
static uint32_t ampOtaCliSeq = 0;

static const char *stateName(OtgState state) {
  switch (state) {
    case IDLE: return "IDLE";
    case PROBE: return "PROBE";
    case WAIT_VBUS: return "WAIT_VBUS";
    case WAIT_HANDSHAKE: return "WAIT_HANDSHAKE";
    case HOST_ACTIVE: return "HOST_ACTIVE";
    case BACKOFF: return "BACKOFF";
    case COOLDOWN: return "COOLDOWN";
    default: return "?";
  }
}

static void logEvent(const String &msg) {
  Serial.print("[OTG] ");
  Serial.println(msg);
}

static void setLedPattern(LedChannel &led, LedPattern pattern, uint32_t now) {
  if (led.pattern != pattern) {
    led.pattern = pattern;
    led.patternStart = now;
  }
}

static bool patternIsOn(const LedChannel &led, uint32_t now) {
  switch (led.pattern) {
    case LED_PATTERN_SOLID:
      return true;
    case LED_PATTERN_BLINK_FAST:
      return (((now - led.patternStart) / 200U) % 2U) == 0U;
    case LED_PATTERN_BLINK_SLOW:
      return (((now - led.patternStart) / 1000U) % 2U) == 0U;
    case LED_PATTERN_OFF:
    default:
      return false;
  }
}

static void updateLedOutputs(uint32_t now) {
  bool redOn = patternIsOn(redLed, now);
  if (redOn != redLed.outputState) {
    redLed.outputState = redOn;
    digitalWrite(PIN_LED_R, redOn ? HIGH : LOW);
  }

  bool greenOn = patternIsOn(greenLed, now);
  if (greenOn != greenLed.outputState) {
    greenLed.outputState = greenOn;
    digitalWrite(PIN_LED_G, greenOn ? HIGH : LOW);
  }
}

static void applyIndicators(uint32_t now) {
  if (panelOtaIsActive()) {
    setLedPattern(redLed, LED_PATTERN_OFF, now);
    setLedPattern(greenLed, LED_PATTERN_BLINK_FAST, now);
    return;
  }

  switch (otgState) {
    case PROBE:
      setLedPattern(redLed, LED_PATTERN_BLINK_FAST, now);
      setLedPattern(greenLed, LED_PATTERN_OFF, now);
      break;
    case WAIT_VBUS:
      setLedPattern(redLed, LED_PATTERN_SOLID, now);
      setLedPattern(greenLed, LED_PATTERN_OFF, now);
      break;
    case WAIT_HANDSHAKE:
      setLedPattern(redLed, LED_PATTERN_SOLID, now);
      setLedPattern(greenLed, LED_PATTERN_SOLID, now);
      break;
    case HOST_ACTIVE:
      setLedPattern(redLed, LED_PATTERN_OFF, now);
      setLedPattern(greenLed, LED_PATTERN_SOLID, now);
      break;
    case BACKOFF:
    case COOLDOWN:
      setLedPattern(redLed, LED_PATTERN_BLINK_SLOW, now);
      setLedPattern(greenLed, LED_PATTERN_OFF, now);
      break;
    case IDLE:
    default:
      setLedPattern(redLed, LED_PATTERN_OFF, now);
      setLedPattern(greenLed, LED_PATTERN_OFF, now);
      break;
  }
}

static void resetCycleCounters() {
  pulseCount = 0;
  backoffIdx = 0;
  powerWakeCount = 0;
  currentBackoffMs = 0;
}

static void startNewProbeCycle(uint32_t now) {
  resetCycleCounters();
  probeStartMs = now;
}

static void triggerPowerPulse(uint32_t now, const char *reason) {
  digitalWrite(PIN_TRIG_PWR, LOW);
  powerPulseActive = true;
  powerPulseStartMs = now;
  powerGraceActive = false;
  powerGraceStartMs = 0;
  lastPowerWakeMs = now;
  powerWakeCount++;
  logEvent(String("power_pulse reason=") + reason);
}

static void finishPowerPulse(uint32_t now) {
  if (powerPulseActive && (now - powerPulseStartMs) >= POWER_WAKE_PULSE_MS) {
    digitalWrite(PIN_TRIG_PWR, HIGH);
    powerPulseActive = false;
    powerGraceActive = true;
    powerGraceStartMs = now;
    logEvent("power_pulse_done");
  }
  if (powerGraceActive && (now - powerGraceStartMs) >= POWER_WAKE_GRACE_MS) {
    powerGraceActive = false;
    logEvent("power_grace_done");
  }
}

static bool shouldCooldown(uint32_t now) {
  if (pulseCount > OTG_MAX_PULSES_PER_CYCLE) {
    logEvent("probe_limit_reached");
    return true;
  }
  if ((now - probeStartMs) >= OTG_MAX_PROBE_DURATION_MS && probeStartMs != 0) {
    logEvent("probe_duration_exceeded");
    return true;
  }
  return false;
}

static void updateVbus(uint32_t now) {
  bool raw = digitalRead(PIN_VBUS_SNS);
  if (raw) {
    if (!vbusRawPrev) {
      vbusHighStartMs = now;
      vbusLowStartMs = 0;
    }
    lastVbusHighMs = now;
    vbusDropStartMs = 0;
    if (!vbusValid && (now - vbusHighStartMs) >= OTG_VBUS_DEBOUNCE_MS) {
      vbusValid = true;
      logEvent(String("vbus_valid ms=") + lastVbusHighMs);
    }
  } else {
    if (vbusRawPrev || vbusLowStartMs == 0) {
      vbusLowStartMs = now;
    }
    if (vbusValid) {
      if (vbusDropStartMs == 0) {
        vbusDropStartMs = now;
      }
      if ((now - vbusDropStartMs) >= OTG_VBUS_LOSS_MS) {
        vbusValid = false;
        vbusDropStartMs = 0;
        vbusHighStartMs = 0;
        logEvent("vbus_lost");
      }
    }
  }
  vbusRawPrev = raw;
}

static void setOtgState(OtgState newState, uint32_t now) {
  if (otgState == newState) {
    return;
  }
  OtgState prev = otgState;
  otgState = newState;
  stateMs = 0;

  String msg = String("state ") + stateName(prev) + " -> " + stateName(newState);
  logEvent(msg);

  if (prev == HOST_ACTIVE && newState != HOST_ACTIVE) {
    hostActive = false;
  }
  if (newState == HOST_ACTIVE) {
    hostActive = true;
    powerWakeCount = 0;
  }
  if (newState == PROBE) {
    otgPulseActive = false;
  }
  if (newState == BACKOFF) {
    if (backoffIdx >= OTG_BACKOFF_LEN) {
      currentBackoffMs = OTG_BACKOFF_SCHEDULE_MS[OTG_BACKOFF_LEN - 1];
    } else {
      currentBackoffMs = OTG_BACKOFF_SCHEDULE_MS[backoffIdx];
    }
  }
  if (newState == COOLDOWN) {
    currentBackoffMs = OTG_COOLDOWN_MS;
  }

  applyIndicators(now);
}
static bool canTriggerPowerWake(uint32_t now) {
  if (!POWER_WAKE_ON_FAILURE) {
    return false;
  }
  if (pulseCount < 2) {
    return false;
  }
  if (powerPulseActive || powerGraceActive) {
    return false;
  }
  if (powerWakeCount >= POWER_WAKE_MAX_PER_EVENT) {
    if ((now - lastPowerWakeMs) < POWER_WAKE_COOLDOWN_MS) {
      return false;
    }
    powerWakeCount = 0;
  }
  return true;
}

static void handleIdle(uint32_t now) {
  if (vbusValid) {
    setOtgState(WAIT_HANDSHAKE, now);
  } else {
    startNewProbeCycle(now);
    setOtgState(PROBE, now);
  }
}

static void handleProbe(uint32_t now) {
  if (stateMs == 0) {
    if (pulseCount == 0) {
      probeStartMs = now;
    }
    pulseCount++;
    if (shouldCooldown(now)) {
      setOtgState(COOLDOWN, now);
      return;
    }
    digitalWrite(PIN_USB_ID, LOW);
    otgPulseActive = true;
    logEvent(String("probe_pulse#") + pulseCount);
  }

  if (otgPulseActive && stateMs >= OTG_PULSE_LOW_MS) {
    digitalWrite(PIN_USB_ID, HIGH);
    otgPulseActive = false;
    logEvent("probe_release");
  }

  if (!otgPulseActive && stateMs >= OTG_PULSE_LOW_MS) {
    if (vbusValid) {
      setOtgState(WAIT_VBUS, now);
    } else {
      setOtgState(BACKOFF, now);
    }
  }
}

static void handleWaitVbus(uint32_t now) {
  if (!vbusValid) {
    setOtgState(BACKOFF, now);
    return;
  }
  if (stateMs >= OTG_GRACE_AFTER_VBUS_MS) {
    setOtgState(WAIT_HANDSHAKE, now);
  }
}

static void handleWaitHandshake(uint32_t now) {
  if (!vbusValid) {
    setOtgState(PROBE, now);
    return;
  }
  if (stateMs >= OTG_HANDSHAKE_TIMEOUT_MS) {
    logEvent("handshake_timeout");
    setOtgState(BACKOFF, now);
  }
}

static void handleHostActive(uint32_t now) {
  digitalWrite(PIN_USB_ID, HIGH);
  if (!vbusValid) {
    logEvent("host_active_vbus_lost");
    startNewProbeCycle(now);
    setOtgState(PROBE, now);
  }
}

static void handleBackoff(uint32_t now) {
  finishPowerPulse(now);

  if (stateMs == 0) {
    logEvent(String("backoff_ms=") + currentBackoffMs);
    if (canTriggerPowerWake(now)) {
      triggerPowerPulse(now, "fallback");
    }
  }

  if (powerPulseActive || powerGraceActive) {
    return;
  }

  if (stateMs >= currentBackoffMs) {
    if (backoffIdx + 1 < OTG_BACKOFF_LEN) {
      backoffIdx++;
    }
    setOtgState(PROBE, now);
  }
}

static void handleCooldown(uint32_t now) {
  finishPowerPulse(now);
  if (stateMs == 0) {
    logEvent("cooldown_start");
  }
  if (stateMs >= currentBackoffMs) {
    logEvent("cooldown_end");
    setOtgState(IDLE, now);
  }
}

static void sendAck(bool ok, const char *cmd, const char *error = nullptr) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "ack";
  root["ok"] = ok;
  if (cmd && *cmd) {
    root["cmd"] = cmd;
  }
  if (!ok && error) {
    root["error"] = error;
  }
  serializeJson(doc, Serial);
  Serial.println();
}

static void emitPanelOtaEvent(const char *evt, int seq = -1, const char *error = nullptr) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "panel_ota";
  root["evt"] = evt;
  if (seq >= 0) {
    root["seq"] = seq;
  }
  if (error && *error) {
    root["error"] = error;
  }
  serializeJson(doc, Serial);
  Serial.println();
}

static bool parseUint32(const String &token, uint32_t &out) {
  char *end = nullptr;
  out = static_cast<uint32_t>(strtoul(token.c_str(), &end, 10));
  return end && *end == '\0';
}

static bool parseHex32(const String &token, uint32_t &out) {
  char *end = nullptr;
  out = static_cast<uint32_t>(strtoul(token.c_str(), &end, 16));
  return end && *end == '\0';
}

static bool decodeBase64(const String &input, std::vector<uint8_t> &out) {
  size_t inLen = input.length();
  if (inLen == 0) {
    out.clear();
    return true;
  }
  size_t outLen = ((inLen + 3) / 4) * 3;
  out.resize(outLen);
  size_t actual = 0;
  int ret = mbedtls_base64_decode(out.data(), outLen, &actual,
                                  reinterpret_cast<const unsigned char *>(input.c_str()), inLen);
  if (ret != 0) {
    out.clear();
    return false;
  }
  out.resize(actual);
  return true;
}

static void sendHelloAck() {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "ack";
  root["ok"] = true;
  root["msg"] = "hello_ack";
  root["host"] = "ok";
  serializeJson(doc, Serial);
  Serial.println();
}

static std::vector<String> tokenize(const String &line) {
  std::vector<String> tokens;
  String current;
  for (size_t i = 0; i < line.length(); ++i) {
    char c = line.charAt(i);
    if (isspace(static_cast<unsigned char>(c))) {
      if (current.length() > 0) {
        tokens.push_back(current);
        current = "";
      }
    } else {
      current += c;
    }
  }
  if (current.length() > 0) {
    tokens.push_back(current);
  }
  return tokens;
}

static bool ensurePanelOtaReady(const char *cmd) {
  if (panelOtaIsActive()) {
    sendAck(false, cmd, "panel_ota_active");
    return false;
  }
  return true;
}

static bool ensureAmpOtaReady(const char *cmd) {
  if (panelOtaIsActive()) {
    sendAck(false, cmd, "panel_ota_active");
    return false;
  }
  return true;
}

static void handlePanelOtaBegin(uint32_t size, bool hasCrc, uint32_t crc) {
  if (!ensurePanelOtaReady("panel_ota_begin")) {
    return;
  }
  if (!panelOtaBegin(size, hasCrc ? crc : 0)) {
    emitPanelOtaEvent("begin_err", -1, panelOtaLastError());
    sendAck(false, "panel_ota_begin", panelOtaLastError());
    return;
  }
  panelOtaCliSeq = 0;
  emitPanelOtaEvent("begin_ok");
  sendAck(true, "panel_ota_begin");
}

static void handlePanelOtaWrite(const String &b64, int seqOverride) {
  if (!panelOtaIsActive()) {
    sendAck(false, "panel_ota_write", "panel_ota_not_active");
    return;
  }
  std::vector<uint8_t> decoded;
  if (!decodeBase64(b64, decoded)) {
    emitPanelOtaEvent("write_err", seqOverride, "base64");
    sendAck(false, "panel_ota_write", "base64");
    return;
  }
  int written = panelOtaWrite(decoded.data(), decoded.size());
  if (written < 0) {
    emitPanelOtaEvent("write_err", seqOverride, panelOtaLastError());
    sendAck(false, "panel_ota_write", panelOtaLastError());
    return;
  }
  int seq = seqOverride >= 0 ? seqOverride : static_cast<int>(panelOtaCliSeq++);
  emitPanelOtaEvent("write_ok", seq);
  sendAck(true, "panel_ota_write");
}

static void handlePanelOtaEnd(bool reboot) {
  if (!panelOtaIsActive()) {
    sendAck(false, "panel_ota_end", "panel_ota_not_active");
    return;
  }
  if (!panelOtaEnd(reboot)) {
    emitPanelOtaEvent("end_err", -1, panelOtaLastError());
    sendAck(false, "panel_ota_end", panelOtaLastError());
    return;
  }
  emitPanelOtaEvent("end_ok");
  sendAck(true, "panel_ota_end");
}

static void handlePanelOtaAbort() {
  if (!panelOtaIsActive()) {
    sendAck(false, "panel_ota_abort", "panel_ota_not_active");
    return;
  }
  panelOtaAbort();
  emitPanelOtaEvent("abort_ok");
  sendAck(true, "panel_ota_abort");
}

static void sendJsonToAmp(const String &payload) {
  Serial2.print(payload);
  Serial2.print('\n');
}

static void handleAmpOtaBegin(uint32_t size, const String &crcStr) {
  if (!ensureAmpOtaReady("ota_begin")) {
    return;
  }
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "cmd";
  JsonObject cmd = root["cmd"].to<JsonObject>();
  JsonObject begin = cmd["ota_begin"].to<JsonObject>();
  begin["size"] = size;
  if (crcStr.length() > 0) {
    begin["crc32"] = crcStr;
  }
  String out;
  serializeJson(doc, out);
  sendJsonToAmp(out);
  ampOtaActive = true;
  ampOtaCliSeq = 0;
  sendAck(true, "ota_begin");
}

static void handleAmpOtaWrite(const String &b64) {
  if (!ensureAmpOtaReady("ota_write")) {
    return;
  }
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "cmd";
  JsonObject cmd = root["cmd"].to<JsonObject>();
  JsonObject write = cmd["ota_write"].to<JsonObject>();
  write["seq"] = ampOtaCliSeq++;
  write["data_b64"] = b64;
  String out;
  serializeJson(doc, out);
  sendJsonToAmp(out);
  sendAck(true, "ota_write");
}

static void handleAmpOtaEnd(bool reboot) {
  if (!ensureAmpOtaReady("ota_end")) {
    return;
  }
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "cmd";
  JsonObject cmd = root["cmd"].to<JsonObject>();
  JsonObject end = cmd["ota_end"].to<JsonObject>();
  end["reboot"] = reboot;
  String out;
  serializeJson(doc, out);
  sendJsonToAmp(out);
  ampOtaActive = false;
  sendAck(true, "ota_end");
}

static void handleAmpOtaAbort() {
  if (!ensureAmpOtaReady("ota_abort")) {
    return;
  }
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "cmd";
  JsonObject cmd = root["cmd"].to<JsonObject>();
  cmd["ota_abort"] = true;
  String out;
  serializeJson(doc, out);
  sendJsonToAmp(out);
  ampOtaActive = false;
  sendAck(true, "ota_abort");
}
static void handlePanelCli(const std::vector<String> &tokens) {
  if (tokens.size() < 2) {
    sendAck(false, "panel", "invalid");
    return;
  }
  if (tokens[1] != "ota") {
    sendAck(false, "panel", "unknown_cmd");
    return;
  }
  if (tokens.size() < 3) {
    sendAck(false, "panel", "invalid");
    return;
  }
  const String &sub = tokens[2];
  if (sub == "begin") {
    if (tokens.size() < 5 || tokens[3] != "size") {
      sendAck(false, "panel_ota_begin", "invalid");
      return;
    }
    uint32_t size = 0;
    if (!parseUint32(tokens[4], size)) {
      sendAck(false, "panel_ota_begin", "size");
      return;
    }
    bool hasCrc = false;
    uint32_t crc = 0;
    if (tokens.size() >= 7) {
      if (tokens[5] != "crc32" || !parseHex32(tokens[6], crc)) {
        sendAck(false, "panel_ota_begin", "crc32");
        return;
      }
      hasCrc = true;
    }
    handlePanelOtaBegin(size, hasCrc, crc);
  } else if (sub == "write") {
    if (tokens.size() < 4) {
      sendAck(false, "panel_ota_write", "invalid");
      return;
    }
    int seq = -1;
    size_t dataIndex = 3;
    if (tokens.size() >= 5 && tokens[3] == "seq") {
      uint32_t val = 0;
      if (!parseUint32(tokens[4], val)) {
        sendAck(false, "panel_ota_write", "seq");
        return;
      }
      seq = static_cast<int>(val);
      if (tokens.size() < 6) {
        sendAck(false, "panel_ota_write", "invalid");
        return;
      }
      dataIndex = 5;
    }
    handlePanelOtaWrite(tokens[dataIndex], seq);
  } else if (sub == "end") {
    bool reboot = true;
    if (tokens.size() >= 5) {
      if (tokens[3] == "reboot") {
        reboot = tokens[4] != "off";
      }
    }
    handlePanelOtaEnd(reboot);
  } else if (sub == "abort") {
    handlePanelOtaAbort();
  } else {
    sendAck(false, "panel", "unknown_cmd");
  }
}

static void handleAmpCli(const std::vector<String> &tokens) {
  if (tokens.empty()) {
    return;
  }
  const String &cmd = tokens[0];
  if (cmd == "ota") {
    if (tokens.size() < 2) {
      sendAck(false, "ota", "invalid");
      return;
    }
    const String &sub = tokens[1];
    if (sub == "begin") {
      if (ampOtaActive) {
        sendAck(false, "ota_begin", "amp_ota_active");
        return;
      }
      if (tokens.size() < 4 || tokens[2] != "size") {
        sendAck(false, "ota_begin", "invalid");
        return;
      }
      uint32_t size = 0;
      if (!parseUint32(tokens[3], size)) {
        sendAck(false, "ota_begin", "size");
        return;
      }
      String crcStr;
      if (tokens.size() >= 6) {
        if (tokens[4] != "crc32") {
          sendAck(false, "ota_begin", "crc32");
          return;
        }
        crcStr = tokens[5];
      }
      handleAmpOtaBegin(size, crcStr);
    } else if (sub == "write") {
      if (!ampOtaActive) {
        sendAck(false, "ota_write", "amp_ota_inactive");
        return;
      }
      if (tokens.size() < 3) {
        sendAck(false, "ota_write", "invalid");
        return;
      }
      handleAmpOtaWrite(tokens[2]);
    } else if (sub == "end") {
      bool reboot = true;
      if (tokens.size() >= 4 && tokens[2] == "reboot") {
        reboot = tokens[3] != "off";
      }
      handleAmpOtaEnd(reboot);
    } else if (sub == "abort") {
      handleAmpOtaAbort();
    } else {
      sendAck(false, "ota", "unknown_cmd");
    }
  } else {
    if (ampOtaActive) {
      sendAck(false, cmd.c_str(), "amp_ota_active");
      return;
    }
    sendAck(false, cmd.c_str(), "unknown_cmd");
  }
}

static void handlePanelJson(const JsonDocument &doc) {
  JsonObjectConst rootCmd = doc["cmd"].as<JsonObjectConst>();
  if (rootCmd.isNull()) {
    sendAck(false, "panel", "invalid");
    return;
  }
  if (JsonObjectConst begin = rootCmd["ota_begin"].as<JsonObjectConst>()) {
    uint32_t size = begin["size"] | 0;
    const char *crcStr = begin["crc32"] | nullptr;
    uint32_t crc = 0;
    bool hasCrc = false;
    if (crcStr && *crcStr) {
      if (!parseHex32(String(crcStr), crc)) {
        sendAck(false, "panel_ota_begin", "crc32");
        return;
      }
      hasCrc = true;
    }
    handlePanelOtaBegin(size, hasCrc, crc);
  } else if (JsonObjectConst write = rootCmd["ota_write"].as<JsonObjectConst>()) {
    int seq = write["seq"] | -1;
    const char *data = write["data_b64"] | "";
    handlePanelOtaWrite(String(data), seq);
  } else if (JsonObjectConst end = rootCmd["ota_end"].as<JsonObjectConst>()) {
    bool reboot = end["reboot"] | true;
    handlePanelOtaEnd(reboot);
  } else if (rootCmd["ota_abort"].is<bool>()) {
    handlePanelOtaAbort();
  } else {
    sendAck(false, "panel", "unknown_cmd");
  }
}

static void trackAmpOtaFromJson(const JsonDocument &doc) {
  const char *type = doc["type"] | "";
  if (strcmp(type, "ota") == 0) {
    const char *evt = doc["evt"] | "";
    if (strcmp(evt, "begin_ok") == 0) {
      ampOtaActive = true;
      ampOtaCliSeq = 0;
    } else if (strcmp(evt, "end_ok") == 0 || strcmp(evt, "abort_ok") == 0 || strcmp(evt, "error") == 0) {
      ampOtaActive = false;
    }
  }
}

static void forwardCmdJsonToAmp(const String &line, const JsonDocument &doc) {
  if (panelOtaIsActive()) {
    sendAck(false, "cmd", "panel_ota_active");
    return;
  }
  JsonObjectConst cmd = doc["cmd"].as<JsonObjectConst>();
  if (cmd.isNull()) {
    sendAck(false, "cmd", "invalid");
    return;
  }
  if (cmd["ota_begin"].is<JsonObject>()) {
    ampOtaActive = true;
    ampOtaCliSeq = 0;
  } else if (cmd["ota_end"].is<JsonObject>() || cmd["ota_abort"].is<bool>()) {
    ampOtaActive = false;
  }
  sendJsonToAmp(line);
}

static void handleHostJsonLine(const String &line, uint32_t now) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err) {
    logEvent(String("json_parse_error: ") + err.c_str());
    return;
  }

  const char *type = doc["type"] | "";
  if (strcmp(type, "hello") == 0) {
    lastHelloMs = now;
    logEvent(String("hello_rx ms=") + lastHelloMs);
    sendHelloAck();
    logEvent("hello_ack_sent");
    if (otgState != HOST_ACTIVE) {
      setOtgState(HOST_ACTIVE, now);
    }
    return;
  }

  if (strcmp(type, "panel") == 0) {
    handlePanelJson(doc);
    return;
  }

  if (strcmp(type, "cmd") == 0) {
    forwardCmdJsonToAmp(line, doc);
    return;
  }

  sendJsonToAmp(line);
}

static void handleHostCliLine(const String &line) {
  std::vector<String> tokens = tokenize(line);
  if (tokens.empty()) {
    return;
  }
  if (tokens[0] == "panel") {
    handlePanelCli(tokens);
  } else {
    handleAmpCli(tokens);
  }
}

static void handleHostFrame(const String &line, uint32_t now) {
  String trimmed = line;
  trimmed.trim();
  if (trimmed.isEmpty()) {
    return;
  }
  if (trimmed.length() >= BRIDGE_MAX_FRAME) {
    logEvent("host_frame_too_long");
    return;
  }
  if (trimmed.startsWith("{")) {
    handleHostJsonLine(trimmed, now);
  } else {
    handleHostCliLine(trimmed);
  }
}

static void handleAmpFrame(const String &line, bool forwardToHost) {
  if (forwardToHost) {
    Serial.print(line);
    Serial.print('\n');
  }
  JsonDocument doc;
  if (deserializeJson(doc, line) == DeserializationError::Ok) {
    trackAmpOtaFromJson(doc);
  }
}

static void serviceHostSerial(uint32_t now) {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      handleHostFrame(hostRxBuffer, now);
      hostRxBuffer = "";
    } else if (hostRxBuffer.length() < BRIDGE_MAX_FRAME - 1) {
      hostRxBuffer += c;
    }
  }
}

static void serviceAmpSerial(bool forwardToHost) {
  while (Serial2.available()) {
    char c = static_cast<char>(Serial2.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      handleAmpFrame(ampRxBuffer, forwardToHost);
      ampRxBuffer = "";
    } else if (ampRxBuffer.length() < BRIDGE_MAX_FRAME - 1) {
      ampRxBuffer += c;
    }
  }
}

static void serviceSerial(uint32_t now) {
  serviceHostSerial(now);
  bool forward = !panelOtaIsActive();
  serviceAmpSerial(forward);
}
void setup() {
  pinMode(PIN_USB_ID, OUTPUT);
  digitalWrite(PIN_USB_ID, HIGH);

  pinMode(PIN_TRIG_PWR, OUTPUT);
  digitalWrite(PIN_TRIG_PWR, HIGH);

  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  digitalWrite(PIN_LED_R, HIGH);
  digitalWrite(PIN_LED_G, LOW);

  pinMode(PIN_VBUS_SNS, INPUT);
  pinMode(PIN_UART2_TX, OUTPUT);
  pinMode(PIN_UART2_RX, INPUT);
  pinMode(PIN_AMP_EN, OUTPUT);
  pinMode(PIN_AMP_GPIO0, OUTPUT);
  digitalWrite(PIN_AMP_EN, HIGH);
  digitalWrite(PIN_AMP_GPIO0, HIGH);

  Serial.begin(HOST_SERIAL_BAUD);
  Serial2.begin(AMP_SERIAL_BAUD, SERIAL_8N1, PIN_UART2_RX, PIN_UART2_TX);

  panelOtaInit();

  logEvent("panel_boot");

  if (POWER_WAKE_ON_BOOT) {
    digitalWrite(PIN_TRIG_PWR, LOW);
    delay(POWER_WAKE_PULSE_MS);
    digitalWrite(PIN_TRIG_PWR, HIGH);
    logEvent("power_boot_pulse");
    delay(POWER_WAKE_GRACE_MS);
  }

  hostRxBuffer.reserve(BRIDGE_MAX_FRAME);
  ampRxBuffer.reserve(BRIDGE_MAX_FRAME);

  lastTick = millis();
  stateMs = 0;
  otgState = IDLE;
  applyIndicators(lastTick);
  updateLedOutputs(lastTick);
}

void loop() {
  uint32_t now = millis();
  uint32_t delta = now - lastTick;
  lastTick = now;
  stateMs += delta;

  finishPowerPulse(now);

  bool panelOtaActiveNow = panelOtaIsActive();
  if (panelOtaActiveNow != panelOtaLatched) {
    panelOtaLatched = panelOtaActiveNow;
    applyIndicators(now);
    logEvent(panelOtaActiveNow ? "panel_ota_active" : "panel_ota_idle");
  }

  if (!panelOtaActiveNow) {
    updateVbus(now);

    switch (otgState) {
      case IDLE:
        handleIdle(now);
        break;
      case PROBE:
        handleProbe(now);
        break;
      case WAIT_VBUS:
        handleWaitVbus(now);
        break;
      case WAIT_HANDSHAKE:
        handleWaitHandshake(now);
        break;
      case HOST_ACTIVE:
        handleHostActive(now);
        break;
      case BACKOFF:
        handleBackoff(now);
        break;
      case COOLDOWN:
        handleCooldown(now);
        break;
    }
  } else {
    digitalWrite(PIN_USB_ID, HIGH);
  }

  serviceSerial(now);
  panelOtaTick(now);
  updateLedOutputs(now);
}
