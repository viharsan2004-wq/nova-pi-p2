# NOVA-PI P2 — Hardware connection, interlocks and bring-up

**Nothing in this repository proves your circuit is safe.** Values below that depend on your coil
and parts are marked *(measure/confirm)*. Pins are intentionally unassigned in `include/config.h`.

## 1. Gate-driver connection

```
ESP32 PIN_PULSE_GATE ───────┐
                            ├─► 2-input AND gate (e.g. 74HC08, 3.3 V) ──► TC4420 IN ──┬── 10 kΩ ─ GND
ESP32 PIN_OUTPUT_ENABLE ─┬──┘                                                         (pull-down AT the driver input)
   ARM key-switch (series)│
   10 kΩ pull-down ───────┘ GND
TC4420 OUT ─► gate resistor *(choose; start ~10 Ω)* ─► IRLZ44N gate ──┬── 10 kΩ ─ GND (gate-source)
IRLZ44N source ─► GND (via current-sense shunt *(recommended for bring-up)*)
IRLZ44N drain  ─► coil ─► +12 V via FUSE and MASTER SWITCH
Clamp / damping network across the coil *(design for your coil; not specified here)*
```
- The TC4420 has no enable pin, so the "hardware output-enable" is the AND gate. With the enable low (or the MCU in reset / unpowered, pins floating) the gate cannot be driven; the pull-downs hold the MOSFET off.
- Confirm in the datasheets: TC4420 input threshold vs your AND-gate output level; TC4420 supply (12 V OK within its rating); IRLZ44N V<sub>GS</sub> max vs the driver supply; decoupling at the TC4420.
- `PIN_OUTPUT_ENABLE` should only be able to pass pulses when the ARM switch is closed. Add a **fuse** and a **master switch** in the battery line.
- Firmware behaviour: both pins low at boot; enable is raised only after LEDC is running and the operator pressed START with confirmation; STOP lowers enable first.

## 2. Analog front end (REQUIRED before connecting the ADC)
The coil flyback can be hundreds of volts. **Never connect the coil node to the ESP32-S3 or ADS1256.** The chain must be: coil damping/clamp → resistive attenuation/protection → buffer (TLV9062, 1.8–5.5 V supply, rail-to-rail) with input series resistance and rail clamps → RC → ADS1256 input. Verify on a scope that the signal at the ADC pin never exceeds the ADS1256 absolute-maximum and the *operating* input range for your AVDD, Vref and PGA, **including during the pulse and flyback**. Firmware sample-delay limits do not protect the ADC; only the analog circuit does.

## 3. ADS1256 notes
- Confirm module logic level (3.3 V SPI), crystal (assumed 7.68 MHz), Vref (assumed 2.5 V), and which header pin is AIN0/AINCOM (`ADC_MUX_VALUE`).
- Full-scale ≈ ±2·Vref/PGA. Volts shown in the UI are only correct if `ADC_VREF_VOLTS` is correct.
- **SYNC/PDWN**: strongly recommended to wire to `PIN_ADC_SYNC` (hardware-timed restart, idle HIGH — add a pull-up). If left unassigned, tie it HIGH on the module.
- Scope-verify DRDY goes HIGH right after the SYNC pulse. If it does not, set `ADC_REQUIRE_DRDY_HIGH_AFTER_SYNC 0` knowing samples may be stale.

## 4. Limits are now DERIVED (coil_model.h) - from estimates you should later measure
The firmware no longer uses a fixed placeholder delay. From `config.h` it computes, with a x1.6 uncertainty band on L
(low L for peak current, high L for flyback time and clamp energy):
- peak current `Ipk = V/R (1 - e^(-t R/L))` -> refuses if > `I_PEAK_MAX_A`; mean current -> `I_AVG_MAX_A`
- clamp power `0.5 L Ipk^2 f` -> refuses if > `CLAMP_P_MAX_W`
- flyback dump time `L Ipk / (V_clamp - V_supply)` + `COIL_SETTLE_US` -> **minimum first sample delay**
- refuses a clamp above 85 % of the FET's Vds max, or below supply + 5 V.
Run `python3 tools/coil_calc.py` (no instruments needed) to get L, R and the numbers to paste into `config.h`.

**Important physical consequence.** With the IRLZ44N (55 V) the clamp must stay below ~46 V, so the coil can only
dump at ~28 V: with the default 60 cm / ~450 uH coil the earliest sensible sample is ~115 us at 3.5 A (~50-100 us at 1-3 A).
Small, fast-decaying targets (loose jewelry) are mostly gone by then. A 400-600 V FET with a 400 V clamp brings that to ~30 us.
Changing `FET_VDS_MAX_V` / `DRAIN_CLAMP_V` to match a new FET lets the firmware accept the shorter delay automatically.

**Measuring without an LCR meter or scope (to replace the estimates):**
1. *R*: coil + wire with a multimeter in resistance mode (subtract probe-lead resistance).
2. *L, indirect*: put the coil in series with a known 100 nF C0G/film capacitor and a 10 ohm resistor, drive it from a phone
   tone generator through a headphone cable, and find the frequency where the voltage across the 10 ohm resistor peaks
   (multimeter AC mV). `L = 1 / ((2 pi f)^2 C)`. A cheap LC meter or a DSO138 scope kit is still the better buy.
3. *Flyback time, using the detector itself*: with a very short pulse and the lowest supply, run PI detect with the coil far from metal,
   and sweep "First sample delay" downward. The point where the tap-0 value jumps or the frame turns SATURATED is the flyback tail.
   Set the delay at 1.5x that, and compare with the model's "min first delay" in Diagnostics.

## 4b. Pin map (default, ESP32-S3-DevKitC-1 with ESP32-S3FH4R2)
| Signal | GPIO | Signal | GPIO |
|---|---|---|---|
| PULSE_GATE (to AND gate) | 4 | ADC CS | 10 |
| OUTPUT_ENABLE (to AND gate) | 5 | ADC MOSI (DIN) | 11 |
| ESTOP button (to GND) | 6 | ADC SCK | 12 |
| BUZZER | 7 | ADC MISO (DOUT) | 13 |
| ADC SYNC/PDWN | 8 | ADC RESET (optional) | 14 |
| ADC DRDY | 9 | S&H gate (when built) | 15 (set `PIN_SH_GATE`) |
| OLED SDA / SCL (reserved) | 1 / 2 | Avoided: 0,3,45,46,19,20,26-37,43,44,48 | |
Check your board's silkscreen: GPIO numbers, not pin positions. Duplicate assignments fail the build.

## 5. Bring-up procedure
1. **Firmware only**, no power stage: simulated mode; check dashboard, STOP, calibrate.
2. **ADC test mode** with a safe known low voltage; check ADC ID = 3, counts stable, diagnostics clean.
3. **Pulse stage on a bench**: current-limited supply (e.g. 5 V, ≤100 mA), resistive load (not the coil), scope on gate. Verify: gate low at boot; pulse width and period match settings; STOP kills pulses within one period; closing the ARM switch is required; unplugging Wi-Fi/closing the page stops pulses within `HEARTBEAT_TIMEOUT_MS`; physical STOP works; pulling power to the ESP32 leaves the gate low.
4. Scope the SYNC pin vs the gate: SYNC low pulse starts `delay_us` after gate falls, and **steps through the tap delays pulse by pulse** (tap k at `delay + k*step`). If you have no scope, use ADC test mode to confirm the ADC, then confirm the tap curve on the dashboard with a coin on the coil.
5. **Coil**: low supply voltage first, shortest pulse, shunt measurement, then step up. Add the analog front end and verify its output before ever connecting the ADC to the coil.
6. Only then use PI detect mode; calibrate on clear ground away from metal.

## 6. Known gaps / things to verify
- Not compiled or run on hardware. LEDC output-invert idle level and reading back an LEDC-driven pad via GPIO interrupt target Arduino-ESP32 2.0.x / IDF 4.4 and must be checked on first build: in detect mode the dashboard "gate edges" counter should climb at 1/period. If it stays at 0, jumper the gate output pin to a spare input pin and set `PIN_GATE_SENSE` in `config.h`.
- The requested period is rounded to an integer LEDC frequency (e.g. 1500 us -> 667 Hz); scope the real period.
- Samples/pulse >1 is limited to data rates ≤ 7500 SPS (SPI read time vs conversion period).
- Don't write flash/NVS while pulsing; settings are not persisted by design.
- Serial output is limited to boot and STOP events; keep it that way during operation.

## 7. Sample-and-hold stage (removes the ADS1256 early-sampling limit)
The ADS1256 digitizes through a sinc filter that integrates ~100-150 us after each SYNC, so without a front-end hold it
cannot see an *instant* of the decay. A sample-and-hold freezes the decay at the sample instant and the slow ADC then
reads that frozen value.

```
coil/attenuator -> TLV9062 buffer A -> analog switch (74HC4066-class, or a low-charge-injection one)
                                           -> C_hold 10 nF C0G/film -> TLV9062 buffer B -> RC -> ADS1256 AIN0
ESP32 PIN_SH_GATE ------------------------> switch control (+10 k pull-down so the switch is open/HOLD at boot)
```
- TLV9062 is a dual op-amp: one half for each buffer. Keep the switch supply and signal within its rated range;
  use a rail-clamped, resistor-limited input so flyback can never reach the switch or the ADC.
- Firmware (set `PIN_SH_GATE` = 15 and rebuild): the switch **tracks for `SH_WINDOW_US` (6 us) ending at the sample instant**
  and then holds; ADC SYNC fires `SH_SETTLE_US` later, so the ADC converts the held value. Delay is therefore the true
  sample time; the first delay must still be >= the flyback minimum (+ the 6 us window).
- Hold droop over the ~200 us conversion should be << 1 LSB of your gain setting: with 10 nF and nA-level leakage it is tens of uV. Verify.
- Charge injection from the switch appears as an offset that calibration removes; large injection reduces dynamic range.
- Until this stage exists leave `PIN_SH_GATE` unassigned. The firmware then models each tap as centred ~2.5/SPS (~83 us at 30 kSPS)
  after the delay.
