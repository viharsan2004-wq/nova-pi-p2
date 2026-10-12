#pragma once
/* Pure-C++ coil/power-stage model. Turns the ESTIMATE values in config.h into limits:
 *   peak current (worst case = low L), mean current, clamp power (worst case = high L),
 *   flyback dump time (worst case = high L) -> minimum sample delay.
 * No Arduino dependencies, so it is host-tested. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "config.h"

struct CoilEst { float ipk_a, iavg_a, clamp_w, dump_us, min_delay_us, vds_pk_v; };

static inline CoilEst coilEstimate(uint32_t pulse_us, uint32_t period_us) {
  CoilEst e = {};
  const float Rt  = COIL_R_OHM + SUPPLY_R_OHM + FET_RDS_ON_OHM + SHUNT_OHM;
  const float Llo = COIL_L_UH * 1e-6f / COIL_L_UNCERT, Lhi = COIL_L_UH * 1e-6f * COIL_L_UNCERT;
  const float t = pulse_us * 1e-6f, T = period_us * 1e-6f;
  const float tau = Llo / Rt;
  e.ipk_a  = SUPPLY_V / Rt * (1.0f - expf(-t / tau));
  e.iavg_a = SUPPLY_V / Rt * (t - tau * (1.0f - expf(-t / tau))) / T;
  e.clamp_w = 0.5f * Lhi * e.ipk_a * e.ipk_a / T;
  const float vc = DRAIN_CLAMP_V - SUPPLY_V;                                    /* volts across the coil while clamping */
  e.dump_us = (vc > 1.0f) ? (Lhi * e.ipk_a / vc) * 1e6f : 1e9f;
  e.min_delay_us = e.dump_us + COIL_SETTLE_US;
  e.vds_pk_v = DRAIN_CLAMP_V;
  return e;
}

/* nullptr = OK, else a reason (static buffer). minDelayOut gets the derived minimum. */
static inline const char* coilCheck(uint32_t pulse_us, uint32_t period_us, float* minDelayOut) {
  static char buf[112];
  const CoilEst e = coilEstimate(pulse_us, period_us);
  if (minDelayOut) *minDelayOut = e.min_delay_us;
  if (DRAIN_CLAMP_V > 0.85f * FET_VDS_MAX_V) { snprintf(buf, sizeof buf, "clamp %.0f V > 85%% of FET Vds max (%.0f V): lower DRAIN_CLAMP_V or use a higher-voltage FET", DRAIN_CLAMP_V, FET_VDS_MAX_V); return buf; }
  if (DRAIN_CLAMP_V < SUPPLY_V + 5.0f)     { snprintf(buf, sizeof buf, "clamp voltage must exceed supply + 5 V (supply %.0f V)", SUPPLY_V); return buf; }
  if (e.ipk_a > I_PEAK_MAX_A)  { snprintf(buf, sizeof buf, "peak current ~%.1f A > limit %.1f A: shorten the pulse", e.ipk_a, I_PEAK_MAX_A); return buf; }
  if (e.iavg_a > I_AVG_MAX_A)  { snprintf(buf, sizeof buf, "mean current ~%.2f A > limit %.2f A: lengthen the period", e.iavg_a, I_AVG_MAX_A); return buf; }
  if (e.clamp_w > CLAMP_P_MAX_W) { snprintf(buf, sizeof buf, "clamp power ~%.1f W > %.1f W: lengthen period / shorten pulse", e.clamp_w, CLAMP_P_MAX_W); return buf; }
  return nullptr;
}
