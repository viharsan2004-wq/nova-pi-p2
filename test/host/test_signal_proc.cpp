#include "signal_proc.h"
#include "coil_model.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

static double noise(double a) { return (rand() % 2001 - 1000) / 1000.0 * a; }

int main() {
  srand(1);
  /* ---------- single tap (legacy behaviour) ---------- */
  {
    Settings s = settingsDefaults(); s.taps = 1; s.thr = 2000; s.calFrames = 30;
    const float T1[MAX_TAPS] = {130, 170, 210, 250};
    SignalProcessor sp; sp.configure(s, 16, T1); sp.reset();
    Frame f = {}; bool any = false;
    auto feed = [&](double base, int frames) {
      for (int i = 0; i < frames * 16; i++) if (sp.push(0, (int32_t)(base + (rand() % 401 - 200)), false, &f)) any = true; };
    feed(1000000, 20);
    CHECK(any && !f.baseValid && !f.det, "1-tap: no detection before calibration");
    sp.startCalibration(); feed(1000000, 40);
    CHECK(f.baseValid && f.noise > 1 && f.noise < 50, "1-tap: calibration completes, sane noise");
    feed(1000000, 20); CHECK(!f.det, "1-tap: quiet signal: no detection");
    feed(1010000, 20); CHECK(f.det && f.rel > 100, "1-tap: step above threshold detected");
    feed(1000000, 40); CHECK(!f.det, "1-tap: returns to baseline: released");
    feed(1001000, 40); CHECK(!f.det, "1-tap: below trigger stays off");
    feed(1010000, 10); CHECK(f.det, "1-tap: target detected before fault test");
    for (int i = 0; i < 16; i++) sp.push(0, 8388607, true, &f);
    CHECK(f.sat && !f.det, "1-tap: saturated frame clears detection");
    for (int i = 0; i < 16; i++) sp.pushInvalid(0, &f);
    CHECK(!f.valid && !f.det, "1-tap: all-invalid frame clears detection");
  }

  /* ---------- 4 taps: decay curve, ground balance, tau, distance ---------- */
  {
    Settings s = settingsDefaults(); s.taps = 4; s.thr = 3000; s.calFrames = 60; s.gbMode = 0; s.avg = 8;
    const float T[MAX_TAPS] = {130, 170, 210, 250};
    SignalProcessor sp; sp.configure(s, s.avg, T); sp.reset();
    Frame f = {};
    double soil = 0, target = 0, tauT = 80.0;      /* target decays with tau = 80 us */
    auto frame = [&]() {
      bool done = false;
      for (int i = 0; i < s.avg && !done; i++)
        for (uint8_t k = 0; k < 4; k++) {
          const double v = 2.0e6 + soil * (T[0] / T[k]) + target * exp(-(T[k] - T[0]) / tauT) + noise(150);
          if (sp.push(k, (int32_t)v, false, &f)) done = true;
        }
    };
    /* calibrate while the operator pumps the coil over soil (soil varies +-40000 counts at tap 0) */
    sp.startCalibration();
    for (int i = 0; i < 200 && !f.baseValid; i++) { soil = 40000.0 * sin(i * 0.5); frame(); }
    CHECK(f.baseValid && f.gMeasured, "4-tap: ground balance measured by regression while pumping coil");
    CHECK(fabs(f.g - (T[3] / T[0])) < 0.35, "4-tap: measured g close to the 1/t soil model (t_late/t_early)");
    /* soil changes after calibration must be rejected, target accepted */
    soil = 30000; target = 0; for (int i = 0; i < 40; i++) frame();
    CHECK(!f.det, "4-tap: 30000-count soil change cancelled by ground balance (no false alarm)");
    soil = 0;   target = 60000; for (int i = 0; i < 40; i++) frame();
    CHECK(f.det, "4-tap: target detected");
    CHECK(f.tau_us > 55 && f.tau_us < 115, "4-tap: decay time constant estimated (true 80 us)");
    CHECK(f.tap[0] > f.tap[1] && f.tap[1] > f.tap[2] && f.tap[2] > f.tap[3], "4-tap: decay curve is monotonically falling");
    /* distance reference: amplitude ~ 1/r^6 */
    CHECK(sp.setReference(100.0f), "reference accepted at 100 cm");
    target = 60000.0 / 64.0 * 1.0;                     /* r = 200 cm -> amplitude /2^6 */
    for (int i = 0; i < 60; i++) frame();
    CHECK(f.det || f.rel > 10, "weaker target still visible");
    target = 60000.0; for (int i = 0; i < 60; i++) frame();
    CHECK(f.dist_cm > 85 && f.dist_cm < 115, "distance ~100 cm when amplitude equals the reference");
  }

  /* ---------- coil / power-stage model ---------- */
  {
    float md = 0;
    const char* e = coilCheck(100, 2000, &md);
    CHECK(e == nullptr, "coil model accepts the default 100 us / 2000 us timing");
    CHECK(md > 60 && md < 400 && md < DEF_DELAY_US, "derived minimum delay is below the default first-tap delay");
    CHECK(coilCheck(300, 2000, &md) != nullptr, "300 us pulse refused (peak current above limit)");
    CoilEst a = coilEstimate(100, 2000), b = coilEstimate(150, 2000);
    CHECK(b.ipk_a > a.ipk_a && b.min_delay_us > a.min_delay_us, "longer pulse -> more current and longer flyback");
    printf("      default: Ipk %.2f A, Iavg %.3f A, clamp %.2f W, dump %.0f us, min delay %.0f us\n", a.ipk_a, a.iavg_a, a.clamp_w, a.dump_us, a.min_delay_us);
  }
  printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
  return fails ? 1 : 0;
}
