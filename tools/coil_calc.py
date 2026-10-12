#!/usr/bin/env python3
"""Coil + power-stage estimator for NOVA-PI P2 (no LCR meter or scope needed).
Prints ESTIMATES to paste into include/config.h. Same model as include/coil_model.h.
Usage: python3 tools/coil_calc.py [--diam 0.60] [--turns 16] [--wire 0.8] [--vs 12] [--pulse 100] [--period 2000]
                                  [--vds 55] [--clamp 40] [--rsupply 0.10] [--rfet 0.035] [--shunt 0.05]"""
import argparse, math
ap = argparse.ArgumentParser()
ap.add_argument('--diam', type=float, default=0.60, help='coil diameter, m')
ap.add_argument('--turns', type=int, default=16)
ap.add_argument('--wire', type=float, default=1.0, help='bare wire diameter, mm')
ap.add_argument('--vs', type=float, default=12.0)
ap.add_argument('--pulse', type=float, default=100, help='us')
ap.add_argument('--period', type=float, default=2000, help='us')
ap.add_argument('--vds', type=float, default=55.0, help='FET Vds max (IRLZ44N=55)')
ap.add_argument('--clamp', type=float, default=40.0, help='drain clamp voltage at peak current')
ap.add_argument('--rsupply', type=float, default=0.10)
ap.add_argument('--rfet', type=float, default=0.035)
ap.add_argument('--shunt', type=float, default=0.05)
ap.add_argument('--uncert', type=float, default=1.6)
ap.add_argument('--settle', type=float, default=25.0)
a = ap.parse_args()

R = a.diam / 2; N = a.turns; dw = a.wire * 1e-3
length = math.pi * a.diam * N
rho_cu = 1.72e-8
Rcoil = rho_cu * length / (math.pi * (dw / 2) ** 2)
# bundle cross-section ~ square of side s (enamel adds ~8%)
s = math.sqrt(N) * dw * 1.08 * 1.1
mu0 = 4e-7 * math.pi
rho_eq = 0.2235 * (2 * s)                       # equivalent radius of a square bundle (Maxwell)
L_neumann = mu0 * R * N ** 2 * (math.log(8 * R / rho_eq) - 2)
Ri, bi = R / 0.0254, s / 0.0254                 # Wheeler (inches)
L_wheeler = 0.8 * (Ri * N) ** 2 / (6 * Ri + 9 * bi + 10 * bi) * 1e-6
L = 0.5 * (L_neumann + L_wheeler)
print(f"wire length {length:.1f} m   R_coil {Rcoil:.2f} ohm   L: Neumann {L_neumann*1e6:.0f} uH, Wheeler {L_wheeler*1e6:.0f} uH -> use {L*1e6:.0f} uH (+-{(a.uncert-1)*100:.0f}% band)")

Rt = Rcoil + a.rsupply + a.rfet + a.shunt
Llo, Lhi = L / a.uncert, L * a.uncert
t, T = a.pulse * 1e-6, a.period * 1e-6
tau = Llo / Rt
ipk = a.vs / Rt * (1 - math.exp(-t / tau))
iavg = a.vs / Rt * (t - tau * (1 - math.exp(-t / tau))) / T
vc = a.clamp - a.vs
dump = Lhi * ipk / vc * 1e6 if vc > 1 else float('inf')
pclamp = 0.5 * Lhi * ipk ** 2 / T
print(f"pulse {a.pulse:.0f} us @ {a.vs:.0f} V: Ipk(worst) {ipk:.2f} A, Imean {iavg:.3f} A, clamp power {pclamp:.2f} W")
print(f"flyback dump (worst) {dump:.0f} us  ->  minimum first sample delay ~ {dump + a.settle:.0f} us")
if a.clamp > 0.85 * a.vds: print(f"!! clamp {a.clamp:.0f} V exceeds 85% of FET Vds max {a.vds:.0f} V")
print("\nPaste into config.h:")
print(f"  #define COIL_L_UH      {L*1e6:.0f}.0f\n  #define COIL_R_OHM     {Rcoil:.2f}f\n  #define SUPPLY_V       {a.vs:.1f}f\n  #define FET_VDS_MAX_V  {a.vds:.0f}.0f\n  #define DRAIN_CLAMP_V  {a.clamp:.0f}.0f")
print("\nWhat-if table (first-delay floor vs. power stage):")
for name, vds, clamp in (("IRLZ44N 55 V, 40 V clamp", 55, 40), ("400-600 V FET, 400 V clamp", 600, 400)):
    for i_pk in (1.0, 2.0, 3.0):
        d = Lhi * i_pk / (clamp - a.vs) * 1e6 + a.settle
        print(f"  {name:28s} Ipk {i_pk:.0f} A -> min delay ~ {d:5.0f} us")
