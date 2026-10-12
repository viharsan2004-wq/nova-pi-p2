/*
 * NOVA-PI P2 - simple pulse-induction metal detector firmware (ESP32-S3, Arduino-ESP32 2.0.x)
 * Pulse timing is hardware (LEDC). ADC reads run in a dedicated task. Web UI runs in loop().
 * Read docs/HARDWARE_AND_SAFETY.md BEFORE connecting a coil.
 */
#include <Arduino.h>
#include <math.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "pulse_ctrl.h"
#include "acq.h"
#include "coil_model.h"
#include "web_ui.h"

static WebServer http(80);
static WebSocketsServer ws(81);
static bool s_buzzer = true;

static float fin(float v) { return (v != v || v > 3.0e38f || v < -3.0e38f) ? 0.0f : v; }

static void sendStatus(int num = -1) {
  AcqStatus st; acq_status(st);
  Settings s; acq_getSettings(s);
  s_buzzer = s.buzzer;
  const float lsb = (2.0f * ADC_VREF_VOLTS / (float)(1 << s.gain)) / 8388607.0f;
  const CoilEst ce = coilEstimate(s.pulse_us, s.period_us);
  char b[1500];
  snprintf(b, sizeof(b),
    "{\"t\":\"s\",\"mode\":%u,\"rq\":%u,\"pulsing\":%d,\"pp\":%d,\"ap\":%d,\"adc\":%d,\"id\":%u,\"err\":\"%s\","
    "\"rd\":%lu,\"to\":%lu,\"lt\":%lu,\"sa\":%lu,\"mi\":%lu,\"ne\":%lu,\"ed\":%lu,\"fr\":%lu,\"sat\":%lu,\"inv\":%lu,\"lsb\":%.6e,"
    "\"set\":{\"pw\":%lu,\"pr\":%lu,\"dl\":%lu,\"spp\":%u,\"dr\":%u,\"g\":%u,\"avg\":%u,\"al\":%.3f,\"thr\":%.1f,\"hy\":%.2f,\"cf\":%u,\"dft\":%.4f,\"cal\":%u,\"tp\":%u,\"ds\":%u,\"gb\":%u,\"dx\":%.1f},\"coil\":{\"ipk\":%.2f,\"iavg\":%.3f,\"cw\":%.2f,\"md\":%.0f,\"sh\":%d}}",
    (unsigned)st.mode, (unsigned)st.req, st.pulsing, cfgPulsePinsOk(), cfgAdcPinsOk(), st.adcOk, (unsigned)st.adcId, st.err,
    (unsigned long)st.adcReads, (unsigned long)st.adcTimeouts, (unsigned long)st.late, (unsigned long)st.syncAnom,
    (unsigned long)st.missed, (unsigned long)st.noEdge, (unsigned long)st.edges, (unsigned long)st.frames,
    (unsigned long)st.satFrames, (unsigned long)st.invalid, (double)lsb,
    (unsigned long)s.pulse_us, (unsigned long)s.period_us, (unsigned long)s.delay_us, s.spp, s.drate, s.gain, s.avg,
    (double)s.alpha, (double)s.thr, (double)s.hyst, s.confirm, (double)s.drift, s.calFrames,
    s.taps, s.dstep_us, s.gbMode, (double)s.dexp,
    (double)ce.ipk_a, (double)ce.iavg_a, (double)ce.clamp_w, (double)ce.min_delay_us, (int)(PIN_SH_GATE >= 0));
  if (num < 0) ws.broadcastTXT(b); else ws.sendTXT((uint8_t)num, b);
}

static void sendFrame(const Frame& f) {
  char b[640];
  int n = snprintf(b, sizeof(b),
    "{\"t\":\"f\",\"n\":%lu,\"raw\":%.1f,\"filt\":%.1f,\"base\":%.1f,\"d\":%.1f,\"rel\":%.1f,\"sg\":%.1f,\"nz\":%.1f,"
    "\"det\":%u,\"sat\":%u,\"ok\":%u,\"cal\":%u,\"cp\":%u,\"bv\":%u,\"gb\":%u,\"gm\":%u,\"g\":%.3f,\"tau\":%.1f,\"dist\":%.0f,\"tp\":[",
    (unsigned long)f.seq, fin(f.raw), fin(f.filt), fin(f.base), fin(f.delta), fin(f.rel), fin(f.sigma), fin(f.noise),
    f.det, f.sat, f.valid, f.cal, f.calPct, f.baseValid, f.gbOn, f.gMeasured, fin(f.g), fin(f.tau_us), fin(f.dist_cm));
  for (uint8_t k = 0; k < f.taps && k < MAX_TAPS && n > 0 && n < (int)sizeof(b) - 24; k++)
    n += snprintf(b + n, sizeof(b) - n, "%s%.0f", k ? "," : "", fin(f.tap[k]));
  if (n > 0 && n < (int)sizeof(b) - 4) snprintf(b + n, sizeof(b) - n, "]}");
  ws.broadcastTXT(b);
  if (PIN_BUZZER >= 0) digitalWrite(PIN_BUZZER, (f.det && s_buzzer) ? HIGH : LOW);
}

static void handleCmd(uint8_t* payload, size_t len) {
  StaticJsonDocument<768> doc;
  if (deserializeJson(doc, (const char*)payload, len)) return;
  acq_heartbeat();
  const char* cmd = doc["cmd"] | "";
  const char* err = nullptr;

  if (!strcmp(cmd, "hb")) return;                       /* heartbeat only */
  if (!strcmp(cmd, "stop")) { acq_stop("operator STOP"); }
  else if (!strcmp(cmd, "start")) {
    const char* m = doc["mode"] | "";
    AcqMode mode = MODE_IDLE;
    if (!strcmp(m, "sim")) mode = MODE_SIM;
    else if (!strcmp(m, "test")) mode = MODE_ADC_TEST;
    else if (!strcmp(m, "detect")) mode = MODE_DETECT;
    if (mode == MODE_IDLE) err = "unknown mode";
    else if (mode == MODE_DETECT && !(doc["confirm"] | false)) err = "coil pulsing needs explicit operator confirmation";
    else acq_start(mode, &err);
  }
  else if (!strcmp(cmd, "calibrate")) acq_calibrate();
  else if (!strcmp(cmd, "reset_baseline")) acq_resetBaseline();
  else if (!strcmp(cmd, "gref")) { const float cm = doc["cm"] | 0.0f; if (cm > 0) acq_setReference(cm); else err = "distance must be > 0"; }
  else if (!strcmp(cmd, "set")) {
    Settings s; acq_getSettings(s);
    if (doc.containsKey("pw"))  s.pulse_us  = doc["pw"].as<uint32_t>();
    if (doc.containsKey("pr"))  s.period_us = doc["pr"].as<uint32_t>();
    if (doc.containsKey("dl"))  s.delay_us  = doc["dl"].as<uint32_t>();
    if (doc.containsKey("tp"))  s.taps      = doc["tp"].as<uint8_t>();
    if (doc.containsKey("ds"))  s.dstep_us  = doc["ds"].as<uint16_t>();
    if (doc.containsKey("gb"))  s.gbMode    = doc["gb"].as<uint8_t>();
    if (doc.containsKey("dx"))  s.dexp      = doc["dx"].as<float>();
    if (doc.containsKey("spp")) s.spp       = doc["spp"].as<uint8_t>();
    if (doc.containsKey("dr"))  s.drate     = doc["dr"].as<uint8_t>();
    if (doc.containsKey("g"))   s.gain      = doc["g"].as<uint8_t>();
    if (doc.containsKey("avg")) s.avg       = doc["avg"].as<uint16_t>();
    if (doc.containsKey("al"))  s.alpha     = doc["al"].as<float>();
    if (doc.containsKey("thr")) s.thr       = doc["thr"].as<float>();
    if (doc.containsKey("hy"))  s.hyst      = doc["hy"].as<float>();
    if (doc.containsKey("cf"))  s.confirm   = doc["cf"].as<uint8_t>();
    if (doc.containsKey("dft")) s.drift     = doc["dft"].as<float>();
    if (doc.containsKey("cal")) s.calFrames = doc["cal"].as<uint16_t>();
    acq_setSettings(s, &err);
  }
  if (err) Serial.printf("cmd '%s' rejected: %s\n", cmd, err);
  sendStatus();
}

static void onWs(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED:    acq_heartbeat(); sendStatus(num); break;
    case WStype_DISCONNECTED: if (ws.connectedClients() == 0) acq_stop("last client disconnected"); break;
    case WStype_TEXT:         handleCmd(payload, len); break;
    default: break;
  }
}

void setup() {
  pulse_begin();                     /* FIRST: outputs forced to safe state, no pulsing */
  Serial.begin(115200);
  delay(200);
  Serial.println("NOVA-PI P2 booting - pulses DISABLED until operator START");
  if (!cfgPulsePinsOk()) Serial.println("NOTE: pulse pins unassigned in config.h - detect mode unavailable");
  if (!cfgAdcPinsOk())   Serial.println("NOTE: ADC pins unassigned in config.h - only simulated mode available");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, 4);
  Serial.print("AP "); Serial.print(WIFI_AP_SSID); Serial.print("  http://"); Serial.println(WiFi.softAPIP());

  http.on("/", HTTP_GET, []() { http.send_P(200, "text/html", INDEX_HTML); });
  http.on("/stop", HTTP_POST, []() { acq_stop("HTTP STOP"); http.send(200, "text/plain", "STOPPED"); });
  http.onNotFound([]() { http.send(404, "text/plain", "not found"); });
  http.begin();
  ws.begin();
  ws.onEvent(onWs);

  acq_begin();
  esp_task_wdt_init(8, true);        /* firmware hang => reset => pins high-Z => external pull-downs hold gate OFF */
  esp_task_wdt_add(NULL);
}

void loop() {
  esp_task_wdt_reset();
  http.handleClient();
  ws.loop();
  acq_loopService();

  Frame f; int n = 0;
  while (n < 8 && acq_getFrame(f)) { sendFrame(f); n++; }

  static uint32_t lastStatus = 0;
  if (millis() - lastStatus > 1000) { lastStatus = millis(); sendStatus(); }
  delay(1);
}
