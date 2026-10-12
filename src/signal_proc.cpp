#include "signal_proc.h"
#include <math.h>
#include <string.h>

void SignalProcessor::configure(const Settings& s, uint16_t perFrame, const float* tapT_us) {
  p_ = s;
  uint8_t t = s.taps < 1 ? 1 : (s.taps > MAX_TAPS ? MAX_TAPS : s.taps);
  if (perFrame < 1) perFrame = 1;
  bool timesChanged = false;
  for (uint8_t k = 0; k < MAX_TAPS && tapT_us; k++) { if (tapT_[k] != tapT_us[k]) timesChanged = true; tapT_[k] = tapT_us[k]; }
  if (perFrame != per_ || t != taps_) {
    per_ = perFrame; taps_ = t;
    memset(att_, 0, sizeof att_); memset(n_, 0, sizeof n_); memset(acc_, 0, sizeof acc_); sat_ = false;
    filtInit_ = false; baseValid_ = false; det_ = false; calibrating_ = false;
  }
  (void)timesChanged;
}

void SignalProcessor::reset() {
  memset(att_, 0, sizeof att_); memset(n_, 0, sizeof n_); memset(acc_, 0, sizeof acc_); sat_ = false;
  filtInit_ = false; memset(filt_, 0, sizeof filt_); memset(base_, 0, sizeof base_);
  memset(noiseK_, 0, sizeof noiseK_); memset(lastRaw_, 0, sizeof lastRaw_);
  noise_ = 0; g_ = 1; lastS_ = 0; gMeasured_ = false; baseValid_ = false; calibrating_ = false; det_ = false;
  calN_ = 0; memset(calSum_, 0, sizeof calSum_); memset(calSq_, 0, sizeof calSq_); calEL_ = 0;
  above_ = below_ = 0; seq_ = 0; refValid_ = false;
}

void SignalProcessor::startCalibration() {
  calibrating_ = true; calN_ = 0; memset(calSum_, 0, sizeof calSum_); memset(calSq_, 0, sizeof calSq_); calEL_ = 0;
  baseValid_ = false; det_ = false; above_ = below_ = 0;
}

void SignalProcessor::resetBaseline() {
  calibrating_ = false; baseValid_ = false; det_ = false; noise_ = 0; above_ = below_ = 0;
}

bool SignalProcessor::setReference(float cm) {
  if (!baseValid_ || !(cm > 0) || fabsf(lastS_) < 3.0f * noise_) return false;
  refAmp_ = fabsf(lastS_); refCm_ = cm; refValid_ = true;
  return true;
}

bool SignalProcessor::push(uint8_t tap, int32_t code, bool saturated, Frame* out) {
  if (tap >= taps_) return false;
  acc_[tap] += code; n_[tap]++; att_[tap]++;
  if (saturated) sat_ = true;
  return advance(out);
}
bool SignalProcessor::pushInvalid(uint8_t tap, Frame* out) {
  if (tap >= taps_) return false;
  att_[tap]++;
  return advance(out);
}

bool SignalProcessor::advance(Frame* out) {
  for (uint8_t k = 0; k < taps_; k++) if (att_[k] < per_) return false;
  return finalize(out);
}

/* Called when the last calibration frame arrives. */
void SignalProcessor::finishCalibration() {
  const double N = calN_;
  double mean[MAX_TAPS], var[MAX_TAPS];
  for (uint8_t k = 0; k < taps_; k++) {
    mean[k] = calSum_[k] / N;
    var[k] = calSq_[k] / N - mean[k] * mean[k];
    base_[k] = (float)mean[k];
    noiseK_[k] = var[k] > 0 ? (float)sqrt(var[k]) : 0.0f;
    if (noiseK_[k] < 1.0f) noiseK_[k] = 1.0f;
  }
  const uint8_t L = taps_ - 1;
  gMeasured_ = false; g_ = 1.0f;
  double vs = var[0] > 0 ? var[0] : 0;
  if (taps_ > 1) {
    /* Default soil model: viscous soil response ~ 1/t, so early/late = t_late/t_early. */
    g_ = tapT_[L] / tapT_[0];
    const double cov = calEL_ / N - mean[0] * mean[L];
    if (var[L] > 0 && var[0] > 0) {
      const double corr = cov / sqrt(var[0] * var[L]);
      const double gm = cov / var[L];
      /* Only trust a regression when the operator actually moved the coil over soil (strong correlation). */
      if (corr > 0.8 && gm > 0.2 && gm < 20.0) { g_ = (float)gm; gMeasured_ = true; }
    }
    const double gg = g_;
    vs = var[0] - 2.0 * gg * cov + gg * gg * var[L];
  }
  noise_ = vs > 0 ? (float)sqrt(vs) : 0.0f;
  if (noise_ < 1.0f) noise_ = 1.0f;
  baseValid_ = true; calibrating_ = false; above_ = below_ = 0;
}

bool SignalProcessor::finalize(Frame* out) {
  Frame f = {};
  f.seq = ++seq_; f.taps = taps_;
  const uint16_t need = (uint16_t)((per_ + 1) / 2);
  bool valid = true;
  for (uint8_t k = 0; k < taps_; k++) {
    if (n_[k] < need) valid = false;
    else lastRaw_[k] = (float)(acc_[k] / n_[k]);
  }
  f.valid = valid; f.sat = sat_;

  if (!valid || sat_) { det_ = false; above_ = below_ = 0; }   /* never carry a stale target through a fault */

  const uint8_t L = taps_ - 1;
  const bool gbOn = taps_ > 1 && (p_.gbMode == 1 || (p_.gbMode == 0 && gMeasured_));
  float s = 0;

  if (valid && !sat_) {
    for (uint8_t k = 0; k < taps_; k++) {
      if (!filtInit_) filt_[k] = lastRaw_[k];
      else filt_[k] += p_.alpha * (lastRaw_[k] - filt_[k]);
    }
    filtInit_ = true;

    if (calibrating_) {
      calN_++;
      for (uint8_t k = 0; k < taps_; k++) { calSum_[k] += filt_[k]; calSq_[k] += (double)filt_[k] * filt_[k]; }
      if (taps_ > 1) calEL_ += (double)filt_[0] * filt_[L];
      if (calN_ >= p_.calFrames) finishCalibration();
    } else if (baseValid_) {
      const float d0 = filt_[0] - base_[0];
      s = gbOn ? (d0 - g_ * (filt_[L] - base_[L])) : d0;
      lastS_ = s;
      const float ad = fabsf(s), on = p_.thr, off = p_.thr * p_.hyst;
      if (!det_) {
        if (ad >= on) { if (++above_ >= p_.confirm) { det_ = true; below_ = 0; } } else above_ = 0;
      } else {
        if (ad < off) { if (++below_ >= p_.confirm) { det_ = false; above_ = 0; } } else below_ = 0;
      }
      if (!det_ && ad < off) for (uint8_t k = 0; k < taps_; k++) base_[k] += p_.drift * (filt_[k] - base_[k]);
    }
  }

  f.raw = lastRaw_[0]; f.filt = filt_[0]; f.base = baseValid_ ? base_[0] : 0;
  for (uint8_t k = 0; k < taps_; k++) f.tap[k] = baseValid_ ? (filt_[k] - base_[k]) : 0.0f;
  f.delta = baseValid_ ? (gbOn ? (f.tap[0] - g_ * f.tap[L]) : f.tap[0]) : 0.0f;
  f.rel = (p_.thr > 0) ? fabsf(f.delta) / p_.thr * 100.0f : 0.0f;
  f.noise = noise_; f.sigma = (noise_ > 0) ? fabsf(f.delta) / noise_ : 0.0f;
  f.det = (valid && !sat_ && baseValid_ && det_) ? 1 : 0;
  f.cal = calibrating_ ? 1 : 0; f.baseValid = baseValid_ ? 1 : 0;
  f.calPct = calibrating_ ? (uint8_t)(100u * calN_ / (p_.calFrames ? p_.calFrames : 1)) : (baseValid_ ? 100 : 0);
  f.gbOn = gbOn ? 1 : 0; f.gMeasured = gMeasured_ ? 1 : 0; f.g = (taps_ > 1) ? g_ : 0.0f;

  /* Decay time constant from first and last tap (single-exponential assumption). */
  if (f.det && taps_ > 1) {
    const float d0 = f.tap[0], dL = f.tap[L];
    /* gate on the ground-balanced noise: per-tap noise is inflated when calibration included coil pumping */
    if (fabsf(d0) > 4 * noise_ && fabsf(dL) > 3 * noise_ && d0 * dL > 0 && fabsf(d0) > fabsf(dL))
      f.tau_us = (tapT_[L] - tapT_[0]) / logf(d0 / dL);
  }
  /* Distance from the operator-supplied reference (relative; same target/orientation assumed). */
  if (f.det && refValid_ && fabsf(f.delta) > 0 && p_.dexp > 0.5f)
    f.dist_cm = refCm_ * powf(refAmp_ / fabsf(f.delta), 1.0f / p_.dexp);

  memset(acc_, 0, sizeof acc_); memset(n_, 0, sizeof n_); memset(att_, 0, sizeof att_); sat_ = false;
  *out = f;
  return true;
}
