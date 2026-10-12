#include "acq.h"
#include "config.h"
#include "ads1256.h"
#include "pulse_ctrl.h"
#include "coil_model.h"
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <math.h>
#include <string.h>

static ADS1256 s_ads;
static SignalProcessor s_sig;
static QueueHandle_t s_q = nullptr;
static TaskHandle_t s_task = nullptr;
static SemaphoreHandle_t s_mtx = nullptr;
static Settings s_set;                 /* operator settings (guarded by s_mtx) */
static Settings s_run;                 /* sampler-private snapshot */
static volatile AcqMode s_req = MODE_IDLE, s_cur = MODE_IDLE;
static volatile bool s_calReq = false, s_rstReq = false, s_cfgReq = false;
static volatile uint32_t s_lastHb = 0;
static bool s_adcOk = false;
static char s_err[64] = "";
static volatile uint32_t c_late, c_sync, c_missed, c_invalid, c_frames, c_satFrames, c_noEdge;
static uint8_t s_noEdgeRun = 0;
/* decay-curve tap sequencing (hardware-SYNC path): tap used by pulses after the last (re)program */
static uint8_t  s_tap = 0, s_prevTap = 0, s_simTap = 0;
static uint32_t s_progEdge = 0;
static bool     s_progLate = false;
static float    s_tapT[MAX_TAPS] = {130, 170, 210, 250};
static volatile bool s_refReq = false; static volatile float s_refCm = 0;

static void setErr(const char* m) { strlcpy(s_err, m, sizeof(s_err)); }
static void getRun() { xSemaphoreTake(s_mtx, portMAX_DELAY); s_run = s_set; xSemaphoreGive(s_mtx); }
void acq_getSettings(Settings& o) { xSemaphoreTake(s_mtx, portMAX_DELAY); o = s_set; xSemaphoreGive(s_mtx); }
void acq_heartbeat() { s_lastHb = millis(); }

static uint32_t windowUs(const Settings& s) {
  const uint32_t sps = ADS1256::drateToSps(s.drate);
  return ADS1256::settleUs(s.drate) + (uint32_t)(s.spp > 0 ? s.spp - 1 : 0) * (1000000UL / sps) + WINDOW_SAFETY_MARGIN_US
         + ((PIN_SH_GATE >= 0) ? SH_SETTLE_US : 0);
}
static PulseTiming timingOf(const Settings& s) { return PulseTiming{s.pulse_us, s.period_us, s.delay_us, s.taps, s.dstep_us}; }
/* Effective sample time of each tap. With the S&H stage the sample instant is the delay itself;
 * without it the ADS1256 sinc filter integrates a window ending ~4/SPS after SYNC, centred ~2.5/SPS. */
static void computeTapTimes(const Settings& s) {
  const float eff = (PIN_SH_GATE >= 0) ? 0.0f : 2.5e6f / (float)ADS1256::drateToSps(s.drate);
  for (uint8_t k = 0; k < MAX_TAPS; k++) s_tapT[k] = (float)s.delay_us + (float)k * (float)s.dstep_us + eff;
}
static uint16_t perFrame(AcqMode m, const Settings& s) { return m == MODE_ADC_TEST ? ADC_TEST_SAMPLES_PER_FRAME : s.avg; }

/* ---------- frame output ---------- */
static void emit(const Frame& f) {
  c_frames++; if (f.sat) c_satFrames++;
  if (xQueueSend(s_q, &f, 0) != pdTRUE) { Frame d; xQueueReceive(s_q, &d, 0); xQueueSend(s_q, &f, 0); }
}
static void feed(uint8_t tap, int32_t code, bool sat) { Frame f; if (s_sig.push(tap, code, sat, &f)) emit(f); }
static void feedInvalid(uint8_t tap) { c_invalid++; Frame f; if (s_sig.pushInvalid(tap, &f)) emit(f); }
static inline bool isSat(int32_t c) { return c >= ADC_SAT_CODE || c <= -ADC_SAT_CODE; }

/* ---------- mode entry ---------- */
static bool adcEnsure() {
  if (!cfgAdcPinsOk()) { setErr("ADC pins unassigned in config.h"); return false; }
  if (!s_adcOk) {
    s_adcOk = s_ads.begin();
    if (!s_adcOk) { setErr("ADS1256 not responding (check wiring / ID)"); return false; }
  }
  return true;
}

static void enterMode(AcqMode m) {
  pulse_stop();
  s_cur = MODE_IDLE;
  getRun();
  { Settings sc = s_run; if (m == MODE_ADC_TEST || m == MODE_IDLE) sc.taps = 1;
    computeTapTimes(sc); s_sig.configure(sc, perFrame(m, sc), s_tapT); }
  s_sig.reset();
  s_tap = s_prevTap = s_simTap = 0; s_progEdge = 0; s_progLate = false;
  s_noEdgeRun = 0;
  xQueueReset(s_q);
  esp_task_wdt_reset();

  switch (m) {
    case MODE_IDLE: return;
    case MODE_SIM: s_cur = MODE_SIM; return;
    case MODE_ADC_TEST:
      if (!adcEnsure()) { s_req = MODE_IDLE; return; }
      if (!s_ads.configure(ADC_TEST_DRATE, s_run.gain, ADC_BUFFER_ENABLE)) { setErr("ADS1256 configure failed"); s_req = MODE_IDLE; return; }
      s_cur = MODE_ADC_TEST; return;
    case MODE_DETECT: {
      if (!adcEnsure()) { s_req = MODE_IDLE; return; }
      if (!s_ads.configure(s_run.drate, s_run.gain, ADC_BUFFER_ENABLE)) { setErr("ADS1256 configure failed"); s_req = MODE_IDLE; return; }
      const char* e = pulse_validate(timingOf(s_run), windowUs(s_run));
      if (e) { setErr(e); s_req = MODE_IDLE; return; }
      if (s_req != MODE_DETECT) return;                       /* STOP arrived meanwhile */
      s_lastHb = millis();
      if (!pulse_start(timingOf(s_run), xTaskGetCurrentTaskHandle(), &e)) { setErr(e); s_req = MODE_IDLE; return; }
      if (s_req != MODE_DETECT) { pulse_stop(); return; }      /* STOP raced with start */
      s_cur = MODE_DETECT; return;
    }
  }
}

/* ---------- per-mode ticks ---------- */
static void simTick() {
  const double t = esp_timer_get_time() / 1e6;
  const double ph = fmod(t, 20.0);                    /* SIMULATED target every 20 s, tau = 80 us, plus slow soil drift ~1/t */
  const double env = (ph > 8 && ph < 11) ? 40000.0 * sin((ph - 8) / 3.0 * M_PI) : 0.0;
  const double soil = 3000.0 * sin(t * 0.05) + 20000.0 * sin(t * 0.9);   /* coil "bobbing" over soil */
  for (uint8_t k = 0; k < s_run.taps; k++) {
    const double v = 1500000.0 + soil * (s_tapT[0] / s_tapT[k]) + env * exp(-(s_tapT[k] - s_tapT[0]) / 80.0)
                     + (double)((int32_t)(esp_random() % 2001) - 1000) * 4.0;
    feed(k, (int32_t)v, false);
  }
  vTaskDelay(pdMS_TO_TICKS(2));
}

static void testTick() {
  if (!s_ads.waitDrdyLow(200000)) { feedInvalid(0); return; }
  int32_t c; s_ads.readSample(c);
  feed(0, c, isSat(c));
}

static inline void waitUntil(int64_t t) { while (esp_timer_get_time() < t) { } }

static void detectTick() {
  const uint32_t n = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
  if (!n) {
    c_noEdge++;
    if (++s_noEdgeRun >= 20) { s_noEdgeRun = 0; setErr("no gate edges seen - pulse not running / edge sense failed"); acq_stop("no edges"); }
    return;
  }
  s_noEdgeRun = 0;
  if (n > 1) c_missed += n - 1;

  const bool hw = PIN_ADC_SYNC >= 0;
  const uint8_t T = s_run.taps < 1 ? 1 : s_run.taps;
  int64_t t0; uint32_t e;                                  /* latest gate falling edge: time and number */
  if (!pulse_lastEdge(&t0, &e)) { c_late++; feedInvalid(0); return; }

  /* Which delay did THIS pulse use?  Hardware path: the one latched before it (a late re-program
   * affects the pulse after next).  Software path: deterministic from the edge number. */
  const bool skipEdge = hw && s_progLate && e == s_progEdge + 1;
  const uint8_t tap = hw ? (skipEdge ? s_prevTap : s_tap) : (uint8_t)((e - 1) % T);
  const uint32_t dl = s_run.delay_us + (uint32_t)tap * s_run.dstep_us;
  const uint32_t pre = (hw ? (SYNC_PULSE_US + 6) : 0) + ((PIN_SH_GATE >= 0) ? SH_SETTLE_US : 0);
  const int64_t target = t0 + dl + pre;

  if (esp_timer_get_time() > target + LATE_TOLERANCE_US) { c_late++; feedInvalid(tap); return; }
  waitUntil(target);
  if (!hw) s_ads.softSync();
#if ADC_REQUIRE_DRDY_HIGH_AFTER_SYNC
  if (s_ads.drdyIsLow()) { c_sync++; feedInvalid(tap); return; }
#endif
  const uint32_t sps = ADS1256::drateToSps(s_run.drate);
  int64_t sum = 0; uint8_t good = 0; bool sat = false;
  for (uint8_t k = 0; k < s_run.spp; k++) {
    const uint32_t to = (k == 0 ? ADS1256::settleUs(s_run.drate) + 500 : 3000000UL / sps + 200);
    if (!s_ads.waitDrdyLow(to)) break;
    int32_t c; s_ads.readSample(c);
    if (isSat(c)) sat = true;
    sum += c; good++;
  }

  /* Re-program the NEXT period's delay right after the sample (hardware path, >1 tap).
   * The new value latches at the next LEDC overflow. If we got here too late for that overflow,
   * the next pulse still uses the old delay: remember it (skipEdge) instead of mislabelling it. */
  if (hw && T > 1) {
    if (skipEdge) s_progLate = false;                      /* the pulse after this one already uses s_tap */
    else {
      const uint8_t next = (uint8_t)((s_tap + 1) % T);
      pulse_setDelayUs(s_run.delay_us + (uint32_t)next * s_run.dstep_us);
      const bool onTime = (esp_timer_get_time() - t0) < (int64_t)s_run.period_us - (int64_t)s_run.pulse_us - 60;
      s_prevTap = s_tap; s_tap = next; s_progEdge = e; s_progLate = !onTime;
    }
  }

  if (!good) { feedInvalid(tap); return; }
  feed(tap, (int32_t)(sum / good), sat);
}

static void taskFn(void*) {
  esp_task_wdt_add(NULL);
  for (;;) {
    esp_task_wdt_reset();
    const AcqMode r = s_req;
    if (r != s_cur) enterMode(r);
    if (s_cfgReq) { s_cfgReq = false; getRun(); Settings sc = s_run; if (s_cur == MODE_ADC_TEST) sc.taps = 1;
                    computeTapTimes(sc); s_sig.configure(sc, perFrame(s_cur, sc), s_tapT); }
    if (s_refReq) { s_refReq = false; if (!s_sig.setReference(s_refCm)) setErr("reference needs a detected target (> 3 sigma) on a calibrated baseline"); }
    if (s_calReq) { s_calReq = false; s_sig.startCalibration(); }
    if (s_rstReq) { s_rstReq = false; s_sig.resetBaseline(); }
    switch (s_cur) {
      case MODE_SIM: simTick(); break;
      case MODE_ADC_TEST: testTick(); break;
      case MODE_DETECT: detectTick(); break;
      default: vTaskDelay(pdMS_TO_TICKS(20)); break;
    }
  }
}

/* ---------- public API ---------- */
void acq_begin() {
  s_mtx = xSemaphoreCreateMutex();
  s_q = xQueueCreate(16, sizeof(Frame));
  s_set = settingsDefaults();
  xTaskCreatePinnedToCore(taskFn, "acq", 6144, nullptr, 5, &s_task, 1);
}

bool acq_start(AcqMode m, const char** err) {
  if (s_req != MODE_IDLE || s_cur != MODE_IDLE) { *err = "already running - STOP first"; return false; }
  Settings s; acq_getSettings(s);
  if (m == MODE_DETECT) {
    if (!cfgPulsePinsOk()) { *err = "pulse pins unassigned in config.h"; return false; }
    if (!cfgAdcPinsOk())   { *err = "ADC pins unassigned in config.h"; return false; }
    const char* e = pulse_validate(timingOf(s), windowUs(s));
    if (e) { *err = e; return false; }
  }
  if (m == MODE_ADC_TEST && !cfgAdcPinsOk()) { *err = "ADC pins unassigned in config.h"; return false; }
  s_err[0] = 0; s_lastHb = millis();
  c_late = c_sync = c_missed = c_invalid = c_frames = c_satFrames = c_noEdge = 0;
  s_req = m;
  return true;
}

void acq_stop(const char* reason) {
  pulse_stop();                 /* hardware first, in this context */
  s_req = MODE_IDLE;
  if (reason) { Serial.print("STOP: "); Serial.println(reason); }
}

void acq_calibrate()     { s_calReq = true; }
void acq_setReference(float cm) { s_refCm = cm; s_refReq = true; }
void acq_resetBaseline() { s_rstReq = true; }
bool acq_getFrame(Frame& f) { return xQueueReceive(s_q, &f, 0) == pdTRUE; }

void acq_status(AcqStatus& s) {
  s.mode = s_cur; s.req = s_req; s.pulsing = pulse_running(); s.adcOk = s_adcOk; s.adcId = s_ads.diag.id;
  s.adcReads = s_ads.diag.reads; s.adcTimeouts = s_ads.diag.timeouts;
  s.late = c_late; s.syncAnom = c_sync; s.missed = c_missed; s.noEdge = c_noEdge;
  s.edges = pulse_edgeCount(); s.frames = c_frames; s.satFrames = c_satFrames; s.invalid = c_invalid;
  strlcpy(s.err, s_err, sizeof(s.err));
}

bool acq_setSettings(const Settings& n, const char** err) {
  Settings o; acq_getSettings(o);
  const bool active = (s_req != MODE_IDLE) || (s_cur != MODE_IDLE);
  const bool hw = n.pulse_us != o.pulse_us || n.period_us != o.period_us || n.delay_us != o.delay_us || n.taps != o.taps || n.dstep_us != o.dstep_us ||
                  n.spp != o.spp || n.drate != o.drate || n.gain != o.gain;
  if (active && hw) { *err = "STOP before changing pulse / ADC settings"; return false; }
  if (n.taps < 1 || n.taps > MAX_TAPS) { *err = "taps must be 1..MAX_TAPS"; return false; }
  if (n.gbMode > 2) { *err = "ground-balance mode must be 0, 1 or 2"; return false; }
  if (!(n.dexp >= 1.0f && n.dexp <= 8.0f)) { *err = "distance exponent must be 1..8"; return false; }
  if (n.spp < 1 || n.spp > 8) { *err = "samples/pulse must be 1..8"; return false; }
  if (!ADS1256::drateValid(n.drate)) { *err = "invalid ADC data rate"; return false; }
  if (n.gain > 6) { *err = "invalid gain"; return false; }
  if (n.spp > 1 && ADS1256::drateToSps(n.drate) > 7500) { *err = "samples/pulse >1 needs data rate <= 7500 SPS"; return false; }
  if (n.avg < 1 || n.avg > 256) { *err = "average must be 1..256"; return false; }
  if (!(n.alpha >= 0.01f && n.alpha <= 1.0f)) { *err = "alpha must be 0.01..1"; return false; }
  if (!(n.thr > 0.0f) || !isfinite(n.thr)) { *err = "threshold must be finite and > 0"; return false; }
  if (!(n.hyst >= 0.1f && n.hyst <= 0.95f)) { *err = "hysteresis must be 0.1..0.95"; return false; }
  if (n.confirm < 1 || n.confirm > 10) { *err = "confirm frames must be 1..10"; return false; }
  if (!(n.drift >= 0.0f && n.drift <= 0.1f)) { *err = "drift must be 0..0.1"; return false; }
  if (n.calFrames < 5 || n.calFrames > 500) { *err = "calibration frames must be 5..500"; return false; }
  const char* e = pulse_validate(timingOf(n), windowUs(n));
  if (e) { *err = e; return false; }
  xSemaphoreTake(s_mtx, portMAX_DELAY); s_set = n; xSemaphoreGive(s_mtx);
  s_cfgReq = true;
  return true;
}

void acq_loopService() {
  if (pulse_estopTriggered()) { setErr("physical STOP pressed"); acq_stop("physical STOP"); }
  if (s_req == MODE_DETECT && (uint32_t)(millis() - s_lastHb) > HEARTBEAT_TIMEOUT_MS) {
    setErr("heartbeat lost - pulses stopped");
    acq_stop("heartbeat lost");
  }
}
