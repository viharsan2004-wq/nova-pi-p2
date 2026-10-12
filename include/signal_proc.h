#pragma once
#include <stdint.h>
#include "settings.h"

struct Frame {
  uint32_t seq;
  float raw;      /* mean ADC counts, tap 0 */
  float filt;     /* EMA-filtered, tap 0 */
  float base;     /* baseline, tap 0 */
  float delta;    /* DETECTION signal: ground-balanced (or tap-0) filt - base */
  float rel;      /* |delta| / threshold * 100 */
  float sigma;    /* |delta| / calibration noise of the detection signal */
  float noise;    /* calibration noise (counts, 1 sigma) of the detection signal */
  uint8_t det, sat, valid, cal, baseValid, calPct;
  uint8_t taps, gbOn, gMeasured;
  float tap[MAX_TAPS];   /* per-tap (filt - base): the decay curve */
  float g;               /* ground-balance factor: delta = tap0 - g * tap[last] */
  float tau_us;          /* decay time constant from first/last tap (0 = unknown) */
  float dist_cm;         /* distance estimate from the reference (0 = no reference) */
};

class SignalProcessor {
 public:
  /* tapT_us: effective sample time of each tap (used for default soil model + tau). */
  void configure(const Settings& s, uint16_t perFrame, const float* tapT_us);
  void reset();
  void startCalibration();
  void resetBaseline();
  bool setReference(float cm);   /* store current signal amplitude as "target at cm" */
  bool push(uint8_t tap, int32_t code, bool saturated, Frame* out);
  bool pushInvalid(uint8_t tap, Frame* out);
 private:
  bool advance(Frame* out);
  bool finalize(Frame* out);
  void finishCalibration();
  Settings p_ = settingsDefaults();
  uint16_t per_ = 16; uint8_t taps_ = 1;
  float tapT_[MAX_TAPS] = {100, 140, 180, 220};
  uint16_t att_[MAX_TAPS] = {}, n_[MAX_TAPS] = {};
  double acc_[MAX_TAPS] = {}; bool sat_ = false;
  bool filtInit_ = false;
  float filt_[MAX_TAPS] = {}, base_[MAX_TAPS] = {}, noiseK_[MAX_TAPS] = {}, lastRaw_[MAX_TAPS] = {};
  float noise_ = 0, g_ = 1, lastS_ = 0;
  bool gMeasured_ = false, baseValid_ = false, calibrating_ = false, det_ = false;
  uint16_t calN_ = 0; double calSum_[MAX_TAPS] = {}, calSq_[MAX_TAPS] = {}, calEL_ = 0;
  uint8_t above_ = 0, below_ = 0; uint32_t seq_ = 0;
  bool refValid_ = false; float refAmp_ = 0, refCm_ = 0;
};
