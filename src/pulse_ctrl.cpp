/*
 * PI pulse generation - ALL pulse timing is produced by the LEDC hardware peripheral:
 *   channel 0: gate pulse (high for pulse_us every period_us)
 *   channel 1: optional ADS1256 SYNC low pulse at (pulse_us + delay_us), same timer => fixed phase
 * No CPU, Wi-Fi, display or Serial activity can add jitter to these edges.
 * A GPIO interrupt on the gate pin's FALLING edge only wakes the sampler task.
 *
 * STOP = (1) output-enable pin inactive (hardware gate), (2) ledc_stop() forces outputs idle,
 * (3) timer paused. STOP is executed directly in the caller's context.
 */
#include "pulse_ctrl.h"
#include "config.h"
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <soc/gpio_periph.h>
#include <soc/io_mux_reg.h>
#include <esp_idf_version.h>
#include <esp_timer.h>
#include <math.h>
#include <stdio.h>
#include "coil_model.h"

static const ledc_mode_t     L_MODE  = LEDC_LOW_SPEED_MODE;
static const ledc_timer_t    L_TIMER = LEDC_TIMER_0;
static const ledc_channel_t  CH_GATE = LEDC_CHANNEL_0;
static const ledc_channel_t  CH_SYNC = LEDC_CHANNEL_1;
static const ledc_channel_t  CH_SH   = LEDC_CHANNEL_2;
static double   s_tpu = 0;          /* LEDC ticks per microsecond */
static uint32_t s_pulseUs = 0;
static volatile uint32_t s_seq = 0; /* seqlock for (lastEdge, edges) */

static volatile bool     s_running = false, s_estop = false, s_edgeAttached = false, s_chCfg = false;
static volatile uint32_t s_edges = 0;
static volatile int64_t  s_lastEdge = 0;
static TaskHandle_t      s_notify = nullptr;
#define EDGE_PIN ((PIN_GATE_SENSE) >= 0 ? (PIN_GATE_SENSE) : (PIN_PULSE_GATE))

static inline void IRAM_ATTR fastSet(int pin, bool hi) {
  if (pin < 0) return;
  if (pin < 32) { if (hi) GPIO.out_w1ts = (1UL << pin); else GPIO.out_w1tc = (1UL << pin); }
  else          { if (hi) GPIO.out1_w1ts.val = (1UL << (pin - 32)); else GPIO.out1_w1tc.val = (1UL << (pin - 32)); }
}

static void IRAM_ATTR estopIsr() { fastSet(PIN_OUTPUT_ENABLE, OE_INACTIVE_LEVEL != 0); s_estop = true; }

static void IRAM_ATTR edgeIsr() {
  s_seq++;                         /* odd = writing */
  s_lastEdge = esp_timer_get_time();
  s_edges++;
  s_seq++;                         /* even = consistent */
  if (s_notify) {
    BaseType_t hp = pdFALSE;
    vTaskNotifyGiveFromISR(s_notify, &hp);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    portYIELD_FROM_ISR(hp);
#else
    if (hp) portYIELD_FROM_ISR();
#endif
  }
}

void pulse_begin() {
  if (PIN_OUTPUT_ENABLE >= 0) { pinMode(PIN_OUTPUT_ENABLE, OUTPUT); digitalWrite(PIN_OUTPUT_ENABLE, OE_INACTIVE_LEVEL); }
  if (PIN_PULSE_GATE >= 0)    { pinMode(PIN_PULSE_GATE, OUTPUT);    digitalWrite(PIN_PULSE_GATE, LOW); }
  if (PIN_ADC_SYNC >= 0)      { pinMode(PIN_ADC_SYNC, OUTPUT);      digitalWrite(PIN_ADC_SYNC, HIGH); }
  if (PIN_SH_GATE >= 0)       { pinMode(PIN_SH_GATE, OUTPUT);       digitalWrite(PIN_SH_GATE, SH_TRACK_LEVEL ? LOW : HIGH); }  /* HOLD */
  if (PIN_BUZZER >= 0)        { pinMode(PIN_BUZZER, OUTPUT);        digitalWrite(PIN_BUZZER, LOW); }
  if (PIN_ESTOP_BUTTON >= 0)  { pinMode(PIN_ESTOP_BUTTON, INPUT_PULLUP); attachInterrupt(PIN_ESTOP_BUTTON, estopIsr, FALLING); }
}

const char* pulse_validate(const PulseTiming& t, uint32_t w) {
  static char buf[128];
  const uint8_t taps = t.taps < 1 ? 1 : t.taps;
  const uint32_t maxDelay = t.delay_us + (uint32_t)(taps - 1) * t.dstep_us;
  const uint32_t shLead = (PIN_SH_GATE >= 0) ? SH_WINDOW_US : 0;   /* tracking window opens this much before the sample instant */
  if (taps > MAX_TAPS) return "taps must be 1..MAX_TAPS";
  if (taps > 1 && t.dstep_us < 5) return "tap step must be >= 5 us";
  if (t.pulse_us < LIM_PULSE_MIN_US || t.pulse_us > LIM_PULSE_MAX_US)    return "pulse width outside config.h limits";
  if (t.period_us < LIM_PERIOD_MIN_US || t.period_us > LIM_PERIOD_MAX_US) return "period outside config.h limits";
  if (t.delay_us < LIM_DELAY_MIN_US || maxDelay > LIM_DELAY_MAX_US)       return "sample delay (first..last tap) outside config.h limits";
  if ((uint64_t)t.pulse_us * 100 > (uint64_t)t.period_us * LIM_DUTY_MAX_PERCENT) return "duty cycle exceeds LIM_DUTY_MAX_PERCENT";
  if (t.pulse_us + maxDelay + w + PERIOD_MARGIN_US > t.period_us)         return "pulse + last delay + ADC window does not fit in period";
  float minDelay = 0;
  const char* ce = coilCheck(t.pulse_us, t.period_us, &minDelay);          /* peak/mean current, clamp power, Vds */
  if (ce) return ce;
  if ((float)t.delay_us - (float)shLead < minDelay) {
    snprintf(buf, sizeof buf, "first delay too early: coil flyback needs >= %.0f us%s (model estimate)", (double)minDelay + shLead, shLead ? " incl. S&H window" : "");
    return buf;
  }
  return nullptr;
}

bool pulse_start(const PulseTiming& t, TaskHandle_t notifyTask, const char** err) {
  if (!cfgPulsePinsOk()) { *err = "pulse pins unassigned in config.h"; return false; }
  if (s_running) return true;

  pulse_stop();   /* known-safe state first */

  const uint32_t freq = (1000000UL + t.period_us / 2) / t.period_us;
  int res = 14;
  while (res > 8 && ((uint64_t)freq << res) > 80000000ULL) res--;

  ledc_timer_config_t tc = {};
  tc.speed_mode = L_MODE; tc.timer_num = L_TIMER; tc.duty_resolution = (ledc_timer_bit_t)res;
  tc.freq_hz = freq; tc.clk_cfg = LEDC_AUTO_CLK;
  if (ledc_timer_config(&tc) != ESP_OK) { *err = "LEDC timer config failed"; return false; }
  ledc_timer_resume(L_MODE, L_TIMER);
  const uint32_t actual = ledc_get_freq(L_MODE, L_TIMER);
  const double tpu = (double)(1UL << res) * (double)actual / 1e6;   /* LEDC ticks per microsecond */
  s_tpu = tpu; s_pulseUs = t.pulse_us;
  const uint32_t shSettle = (PIN_SH_GATE >= 0) ? SH_SETTLE_US : 0;

  ledc_channel_config_t cc = {};
  cc.speed_mode = L_MODE; cc.timer_sel = L_TIMER; cc.intr_type = LEDC_INTR_DISABLE;

  if (PIN_ADC_SYNC >= 0) {
    cc.gpio_num = PIN_ADC_SYNC; cc.channel = CH_SYNC;
    cc.duty = (uint32_t)lround(SYNC_PULSE_US * tpu); if (cc.duty < 1) cc.duty = 1;
    cc.hpoint = (uint32_t)lround((t.pulse_us + t.delay_us + shSettle) * tpu);
    cc.flags.output_invert = 1;   /* active-low SYNC pulse, idle HIGH */
    if (ledc_channel_config(&cc) != ESP_OK) { pulse_stop(); *err = "LEDC sync channel config failed"; return false; }
    s_chCfg = true;
  }

  if (PIN_SH_GATE >= 0) {                       /* sample-and-hold switch: tracks for SH_WINDOW_US ending at the sample instant */
    cc.gpio_num = PIN_SH_GATE; cc.channel = CH_SH;
    cc.duty = (uint32_t)lround(SH_WINDOW_US * tpu); if (cc.duty < 1) cc.duty = 1;
    cc.hpoint = (uint32_t)lround((t.pulse_us + t.delay_us - SH_WINDOW_US) * tpu);
    cc.flags.output_invert = SH_TRACK_LEVEL ? 0 : 1;   /* idle (pre-inversion 0) = HOLD (switch open) */
    if (ledc_channel_config(&cc) != ESP_OK) { pulse_stop(); *err = "LEDC S&H channel config failed"; return false; }
    s_chCfg = true;
  }

  cc.flags.output_invert = 0;
  cc.gpio_num = PIN_PULSE_GATE; cc.channel = CH_GATE;
  cc.duty = (uint32_t)lround(t.pulse_us * tpu); if (cc.duty < 1) cc.duty = 1;
  cc.hpoint = 0;
  if (ledc_channel_config(&cc) != ESP_OK) { pulse_stop(); *err = "LEDC gate channel config failed"; return false; }
  s_chCfg = true;

  /* Edge sensing. NOTE: do NOT call gpio_set_direction() on the gate pad here - it can reconnect the pad
   * to the plain GPIO output and disconnect the LEDC signal. Only set the pad's input-enable bit. */
  if (PIN_GATE_SENSE >= 0) pinMode(PIN_GATE_SENSE, INPUT);
  else PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[PIN_PULSE_GATE]);
  s_notify = notifyTask; s_edges = 0; s_lastEdge = 0;
  attachInterrupt(EDGE_PIN, edgeIsr, FALLING);
  s_edgeAttached = true;

  /* Last step: hardware enable. If it arrives mid-pulse the first pulse is merely truncated. */
  s_estop = false;
  s_running = true;
  fastSet(PIN_OUTPUT_ENABLE, OE_ACTIVE_LEVEL != 0);
  return true;
}

void pulse_stop() {
  fastSet(PIN_OUTPUT_ENABLE, OE_INACTIVE_LEVEL != 0);   /* 1. hardware enable OFF */
  s_running = false;
  if (s_edgeAttached && EDGE_PIN >= 0) { detachInterrupt(EDGE_PIN); s_edgeAttached = false; }
  if (s_chCfg) {                                         /* 2. force LEDC outputs idle */
    ledc_stop(L_MODE, CH_GATE, 0);
    if (PIN_ADC_SYNC >= 0) ledc_stop(L_MODE, CH_SYNC, 0); /* inverted channel: idle pin level = HIGH */
    if (PIN_SH_GATE  >= 0) ledc_stop(L_MODE, CH_SH,   0); /* idle = HOLD */
    ledc_timer_pause(L_MODE, L_TIMER);                   /* 3. stop the timer */
    s_chCfg = false;
  }
}

bool     pulse_running()       { return s_running; }
bool     pulse_estopTriggered(){ bool v = s_estop; s_estop = false; return v; }
uint32_t pulse_edgeCount()     { return s_edges; }
int64_t  pulse_lastEdgeUs()    { return s_lastEdge; }

bool pulse_lastEdge(int64_t* t_us, uint32_t* index) {
  for (int i = 0; i < 4; i++) {                       /* seqlock read: retry if the ISR was mid-update */
    const uint32_t a = s_seq;
    const int64_t t = s_lastEdge; const uint32_t n = s_edges;
    const uint32_t b = s_seq;
    if (a == b && !(a & 1)) { *t_us = t; *index = n; return true; }
  }
  return false;
}

void pulse_setDelayUs(uint32_t delay_us) {
  if (!s_running || PIN_ADC_SYNC < 0 || s_tpu <= 0) return;
  const uint32_t shSettle = (PIN_SH_GATE >= 0) ? SH_SETTLE_US : 0;
  uint32_t duty = (uint32_t)lround(SYNC_PULSE_US * s_tpu); if (duty < 1) duty = 1;
  ledc_set_duty_with_hpoint(L_MODE, CH_SYNC, duty, (uint32_t)lround((s_pulseUs + delay_us + shSettle) * s_tpu));
  ledc_update_duty(L_MODE, CH_SYNC);
  if (PIN_SH_GATE >= 0) {
    uint32_t d2 = (uint32_t)lround(SH_WINDOW_US * s_tpu); if (d2 < 1) d2 = 1;
    ledc_set_duty_with_hpoint(L_MODE, CH_SH, d2, (uint32_t)lround((s_pulseUs + delay_us - SH_WINDOW_US) * s_tpu));
    ledc_update_duty(L_MODE, CH_SH);
  }
}
