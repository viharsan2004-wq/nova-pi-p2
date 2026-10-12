# NOVA-PI P2 — Hardware Protection Addendum

**Purpose:** Add a fail-safe hardware interlock and a safe bring-up checklist. This document does not certify the circuit and cannot replace measurements. Do not connect the coil or enable 12 V pulse power until the clamp, analog front end, and MOSFET stress have been verified with suitable test equipment.

## 1. Minimum protection chain

```text
12 V battery (+)
   |
  F1  DC-rated fuse, mounted close to battery (+)
   |
  S1  master ON/OFF switch (DC-current rated)
   |
  +---------------------------> coil supply / pulse stage (exact topology per reviewed schematic)
   |
  DC-rated current-limited test supply for initial testing

ESP32 GPIO PULSE_GATE ----+      HARDWARE, not software-only
                          AND gate ----> TC4420 IN ---- gate resistor ---- MOSFET gate
ESP32 GPIO OUTPUT_ENABLE -+                    |                         |
                                              10 kΩ pull-down           10 kΩ gate-source pull-down
                                              at driver input          at MOSFET pins

Physical ARM switch/key in series with OUTPUT_ENABLE signal; 10 kΩ pull-down on the enable net.
Physical STOP button must force OUTPUT_ENABLE inactive independently of firmware.
All logic grounds and power grounds join at a deliberate low-impedance point; keep coil current return away from the analog front-end return.
```

Use a 3.3 V-compatible AND gate only after checking its input thresholds and supply range. The ARM switch must be a real hardware interruption of the enable path, not merely a button read by the ESP32. The STOP path should be normally closed / fail-safe where practical, so a broken wire disables the driver. Confirm the exact switch logic and polarity before power-up.

## 2. Power input and current protection

- Fit a fuse in the battery-positive lead as close to the battery terminal as practical. Choose a **DC-rated** fuse and holder whose interrupt rating, voltage rating, and time-current curve suit the battery and wiring.
- Do **not** choose a fuse value just from the MOSFET name or battery Ah rating. First measure/limit the pulse current and calculate wiring, connector, coil, MOSFET, and fuse coordination. The fuse protects against wiring faults and sustained overcurrent; it is not fast enough to protect a MOSFET from every pulse fault.
- Use a master switch rated for the DC current and inductive environment. Do not connect/disconnect the coil while pulsing.
- Add strain relief, insulation, and covered terminals. Keep the battery physically separated from exposed coil-stage conductors.
- Initial power-up: use a current-limited bench supply at low voltage and a **non-inductive resistive dummy load**, not the coil. Do not use a lead-acid battery for first tests because it can deliver very high fault current.

## 3. MOSFET gate fail-safe

- Keep a 10 kΩ gate-to-source pull-down physically close to the MOSFET.
- Keep a 10 kΩ pull-down on the TC4420 input, physically close to the driver input.
- Use a gate resistor near the MOSFET gate; the final value must be confirmed by checking switching edges and ringing on an oscilloscope.
- Place the TC4420's local bypass capacitor directly between its supply and ground pins, following its datasheet layout guidance. Keep driver-to-gate and gate-return paths short.
- The ESP32 must never drive the MOSFET gate directly. Its GPIO drives only the logic/interlock input.
- Confirm the TC4420 supply voltage and the MOSFET's maximum gate-source voltage are compatible. Do not assume the driver's supply is safe for the MOSFET gate without checking the actual circuit.

## 4. Coil flyback/clamp — critical, not optional

The coil can generate a high-voltage transient when switched off. A software delay, fuse, or op-amp input resistor does **not** clamp this energy.

- A clamp/damping network must be designed for the measured coil inductance `L`, peak current `Ipk`, pulse repetition rate, and energy per pulse `E = 0.5 × L × Ipk²`.
- Verify clamp voltage at the MOSFET drain, repetitive pulse-energy rating, average power, thermal rise, and transient overshoot on an oscilloscope with an appropriately rated probe.
- If using an IRLZ44N, its drain-source rating is only 55 V. The total drain peak including wiring overshoot must remain safely below the absolute maximum. The project's model limit is only an estimate, not proof of safety.
- **Do not use the two 1N5408 diodes as the coil's high-energy PI clamp by default.** They are general-purpose rectifiers, and their speed/energy suitability for this repetitive pulse circuit has not been established. Do not substitute a random TVS, Schottky diode, or zener without calculating and measuring the clamp behavior.
- If you do not have a scope or a reviewed clamp design, keep the coil disconnected and do not run pulse mode. Have the clamp stage checked by someone equipped to measure inductive switching safely.

## 5. Protect the ADS1256 and TLV9062

```text
coil / drain node  -- NEVER connect directly --> ADS1256 or ESP32

coil response --> engineered clamp + attenuation/current limiting
              --> input clamps referenced to the analog rails (designed for signal current)
              --> TLV9062 buffer / RC filter
              --> ADS1256 input
```

- The ADS1256 input must stay within both its recommended operating range and absolute maximum ratings, including startup, pulse, flyback, and fault conditions. Check the module's AVDD, DVDD, reference, PGA, and input common-mode requirements.
- The TLV9062 is not a high-voltage transient protector. Input resistors and clamp diodes must be sized so the op-amp's input current and rail-injection current remain within ratings. Do not assume its internal protection can absorb coil energy.
- Use separate routing for high-current coil/driver loops and sensitive analog signals. Join grounds intentionally; do not route coil current through the ADS1256/TLV9062 ground return.
- Add local decoupling at each IC, short analog traces, and a defined analog input bias/return path. Verify the front-end output with the coil disconnected first, then with a safely limited test signal.

## 6. Required commissioning gates

Do not proceed to the next step until the previous one passes.

1. **Power off visual check:** verify polarity, solder bridges, fuse placement, pull-downs, ground continuity, and that the coil is disconnected.
2. **MCU only:** power ESP32 by USB; confirm PULSE_GATE and OUTPUT_ENABLE stay inactive through reset, boot, Wi-Fi connection, and browser disconnect/reconnect.
3. **Interlock test:** with the 12 V power stage disconnected, verify ARM off blocks the AND gate, STOP forces enable inactive, and a broken/unplugged enable wire disables pulses.
4. **Driver test:** use a current-limited low-voltage supply and resistive dummy load; verify the TC4420 output and MOSFET gate on a scope. Check boot, reset, STOP, and software crash behavior.
5. **Clamp review:** calculate and measure the actual coil energy and drain waveform with correctly rated probes. Stop if there is ringing/overshoot or the measured peak approaches the device rating.
6. **Analog test:** coil disconnected; apply a known low-voltage test signal to the protected front end and verify no ADC pin exceeds its limits.
7. **Low-energy coil test:** only after steps 1–6 pass, begin at the lowest practical supply and pulse energy while monitoring current, drain voltage, clamp temperature, MOSFET temperature, and analog output.

## 7. Parts not safely specifiable from the current information

The exact fuse rating, clamp part/value, clamp resistor, power rating, and final gate resistor cannot be responsibly finalized from coil diameter/wire length alone. Needed measurements are coil resistance, inductance, peak current, pulse width/repetition rate, actual circuit topology, and measured drain waveform. A 50–60 cm coil's dimensions do not uniquely determine inductance or current.

**Stop condition:** If the drain waveform, coil current, or ADC input voltage cannot be measured, do not operate the coil from the 12 V battery. Keep firmware in simulated/ADC-only mode until the hardware is validated.
