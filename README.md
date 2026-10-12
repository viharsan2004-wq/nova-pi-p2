# NOVA-PI P2 v2 — Pulse-induction metal detector firmware (multi-delay decay curve)

ESP32-S3 · Arduino-ESP32 2.0.x (PlatformIO) · ADS1256 · IRLZ44N + TC4420 · Wi-Fi AP dashboard

> **Status: UNTESTED ON HARDWARE.** Signal processing and the coil model are host-tested. The ESP32 files were only
> syntax-checked against stub headers (not the real Arduino-ESP32 toolchain) and have **not been run**; the per-pulse
> LEDC delay re-programming in particular must be scope-verified. Expect to fix
> small compile errors. Compiling does not make the system electrically safe. Read
> `docs/HARDWARE_AND_SAFETY.md` before connecting a coil.

## Quick start (no hardware needed)
1. `pio run -t upload` (board `esp32-s3-devkitc-1`; uncomment the USB flags in `platformio.ini` if you use the native USB port).
2. Join Wi-Fi **NOVA-PI-P2** (password in `include/config.h` — change it), open **http://192.168.4.1**.
3. Choose **Simulated data**, press START, then CALIBRATE. The simulated target appears every 20 s. Everything is labelled SIMULATED.

## Going to real hardware (in this order)
1. Edit `include/config.h`: check the default GPIO map against your board, confirm ADC crystal, Vref, MUX channel, and enter your coil / FET / clamp numbers (`tools/coil_calc.py`).
2. **ADC test mode** (pulses forced OFF): connect a safe known low voltage (e.g. a 1.5 V cell via divider) and verify counts/ID/diagnostics.
3. Bench-test the pulse stage with a **resistive load, current-limited supply, and a scope** — never the coil first.
4. Only with a verified clamp + analog front end: **PI detect** mode.

## Architecture
| Piece | Where | Notes |
|---|---|---|
| Pulse + ADC-sync timing | LEDC hardware (`pulse_ctrl.cpp`) | gate pulse and ADS1256 SYNC pulse share one timer ⇒ fixed phase, zero CPU jitter |
| ADC sampling | FreeRTOS task, core 1 (`acq.cpp`) | woken by gate falling edge; discards late/stale samples and counts them |
| Signal processing | `signal_proc.cpp` | frame average → EMA → baseline subtract → threshold + hysteresis + confirm + drift tracking |
| UI | `web_ui.cpp`, `main.cpp` | HTTP :80, WebSocket :81, canvas charts, no external libraries |

## STOP paths (all independent)
- STOP button → WebSocket **and** `POST /stop`; handler calls the hardware stop directly.
- Heartbeat: dashboard must send one every 0.5 s; pulses stop after `HEARTBEAT_TIMEOUT_MS`.
- Last Wi-Fi client disconnects → stop.
- Optional physical STOP pin (ISR drops the hardware enable immediately).
- Task watchdog: firmware hang ⇒ reset ⇒ GPIOs float ⇒ **external pull-downs must hold the gate off**.
- Hardware enable + external ARM switch / fuse (see safety doc).

## Messages (WebSocket JSON)
Client→device: `hb`, `start{mode,confirm}`, `stop`, `calibrate`, `reset_baseline`, `set{pw,pr,dl,spp,dr,g,avg,al,thr,hy,cf,dft,cal}`, `get`.
Device→client: `f` (frame) and `s` (status + settings + counters).

## What changed in v2
| Problem | Fix |
|---|---|
| Single delay, one number per frame | **1-4 delays sampled in rotation** (one per pulse, hardware-timed SYNC reprogrammed after each sample) -> a **decay curve** per frame, a **decay time constant** (tau), and a **ground-balanced** detection signal (`tap0 - g * tap_last`). `g` is measured by regression if you pump the coil over bare soil during CALIBRATE, otherwise a 1/t soil model is used. |
| Placeholder limits | Limits are **derived** from a coil/power-stage model (`coil_model.h`, `tools/coil_calc.py`): peak/mean current, clamp power, Vds margin, and the **minimum first delay from the flyback dump time**. |
| All GPIO unassigned | Default pin map for ESP32-S3-DevKitC-1 (avoids strapping/USB/flash pins), compile-time duplicate check, pin table in docs. |
| ADS1256 cannot see the early decay | Optional **sample-and-hold control** (`PIN_SH_GATE`): firmware tracks for 6 us, holds, then restarts the ADC. Needs the small analog stage in docs section 7; the hardware part cannot be fixed in software. |
| No depth | Optional **distance estimate** relative to a reference target you place at a known distance (`amplitude ~ 1/r^n`, default n = 6). |

## What this firmware still does NOT do
- It cannot tell gold from other metal. tau and the curve are indicators; they need your own labelled test targets.
- The distance figure is relative to one reference and only valid for a similar target, orientation and soil.
- Ground balance assumes a 1/t soil response; strongly mineralised soil may need the measured (pumped) calibration.
- No settings persistence (defaults reload at boot).
- Derived limits are only as good as the ESTIMATE values in `config.h` (measure L, R and flyback to confirm).
- It cannot verify your analog hardware. The ADS1256's sinc filter integrates ≈150 µs or more after each sync, so it sees the *integrated* decay tail starting at the sample delay, not an instantaneous sample; many PI designs use a sample-and-hold stage in front for fast decay. Evaluate this on a scope with a real target.

## Host tests (no hardware)
```
g++ -std=c++11 -Iinclude test/host/test_signal_proc.cpp src/signal_proc.cpp -o /tmp/sp_test && /tmp/sp_test
```
Exits non-zero on failure. Covers calibration, detection, hysteresis, saturation/invalid frames, 4-tap ground balance
(regression + soil-change rejection), tau estimate, distance reference, and the coil model.
`python3 tools/coil_calc.py` prints the coil/limit estimates.

## Recent reliability update
- Invalid or saturated frames now clear the active detection state, so the buzzer/UI do not keep reporting a stale target through a sensor fault. Detection must be confirmed again on valid frames.
- Detection threshold settings must now be finite and greater than zero.
- Dashboard wording now distinguishes a detected signal response from actual metal identification. The chart is a trend of averaged frames, **not** a full coil-response waveform.

## Metal classification / AI limitation
The current acquisition pipeline averages ADC readings into one value per frame at a configured delay. That is not enough information to reliably classify ferrous vs non-ferrous metal or identify gold. A future classifier needs repeatable multi-delay samples (a decay curve or other independent signal features) and a labelled dataset collected from known targets under varied distances, orientations, ground and coil conditions. Until then, report the target as **unclassified** rather than presenting a guessed metal type. Lightweight adaptive filtering can be evaluated after recording representative real signals; an AI model cannot recover information the hardware never sampled.
