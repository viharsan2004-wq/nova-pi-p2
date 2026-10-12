#pragma once
#include <stdint.h>
#include "config.h"

struct Settings {
  /* Pulse / ADC: may only change while STOPPED */
  uint32_t pulse_us;
  uint32_t period_us;
  uint32_t delay_us;   /* FIRST tap delay (gate fall -> sample instant) */
  uint8_t  taps;       /* number of delays sampled in rotation, 1..MAX_TAPS (decay curve) */
  uint16_t dstep_us;   /* spacing between tap delays */
  uint8_t  spp;        /* ADC samples averaged per pulse */
  uint8_t  drate;      /* ADS1256 DRATE register code */
  uint8_t  gain;       /* PGA code 0..6 */
  /* Processing: may change live */
  uint16_t avg;        /* pulse attempts per tap per output frame */
  float    alpha;      /* EMA filter coefficient (0..1] */
  float    thr;        /* detection threshold, ADC counts (on the ground-balanced signal) */
  float    hyst;       /* release ratio */
  uint8_t  confirm;    /* consecutive frames to switch state */
  float    drift;      /* slow baseline tracking coefficient */
  uint16_t calFrames;  /* frames used for calibration */
  uint8_t  gbMode;     /* 0 auto (on if measured), 1 on, 2 off */
  float    dexp;       /* distance exponent n: amplitude ~ 1/r^n */
  bool     buzzer;
};

static inline Settings settingsDefaults() {
  Settings s;
  s.pulse_us = DEF_PULSE_US; s.period_us = DEF_PERIOD_US; s.delay_us = DEF_DELAY_US;
  s.taps = DEF_TAPS; s.dstep_us = DEF_TAP_STEP_US;
  s.spp = DEF_SAMPLES_PER_PULSE; s.drate = DEF_DRATE; s.gain = DEF_GAIN_CODE;
  s.avg = DEF_AVG_PULSES; s.alpha = DEF_EMA_ALPHA; s.thr = DEF_THRESHOLD_COUNTS;
  s.hyst = DEF_HYSTERESIS; s.confirm = DEF_CONFIRM_FRAMES; s.drift = DEF_DRIFT_ALPHA;
  s.calFrames = DEF_CAL_FRAMES; s.gbMode = DEF_GB_MODE; s.dexp = DEF_DIST_EXP; s.buzzer = true;
  return s;
}
