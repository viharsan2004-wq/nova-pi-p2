#pragma once
/*
 * NOVA-PI P2 v2 - HARDWARE-DEPENDENT CONFIGURATION
 * ---------------------------------------------------------------------------
 * Values marked ESTIMATE come from tools/coil_calc.py or from datasheets, NOT from
 * measurement. The firmware derives its pulse / delay limits from them (coil_model.h)
 * with safety factors, so a wrong ESTIMATE gives a wrong limit. Measure and update
 * them as soon as you can borrow an LCR meter / scope. Compiling does NOT make the
 * system safe.
 */
#include <stdint.h>

#define PIN_UNASSIGNED (-1)

/* ===================== GPIO MAP (ESP32-S3-DevKitC-1, ESP32-S3FH4R2) =========
 * Avoided on purpose: strapping 0/3/45/46, USB 19/20, in-package flash/PSRAM 26-32,
 * 33-37 (octal-PSRAM boards), 48 (on-board RGB LED), 43/44 (UART0).
 * ADS1256 uses the default FSPI pins (10/11/12/13). Reserved for the OLED (I2C): 1 = SDA, 2 = SCL.
 * Change any of these to match YOUR wiring; the compile-time checks below catch duplicates. */
#define PIN_PULSE_GATE     4   /* pulse output -> AND gate input A -> TC4420 IN      */
#define PIN_OUTPUT_ENABLE  5   /* hardware enable -> AND gate input B (see docs)    */
#define PIN_ESTOP_BUTTON   6   /* physical STOP, active LOW, internal pull-up       */
#define PIN_BUZZER         7   /* active buzzer, active HIGH                        */
#define PIN_ADC_SYNC       8   /* ADS1256 SYNC/PDWN (LEDC hardware-timed)           */
#define PIN_ADC_DRDY       9
#define PIN_ADC_CS         10
#define PIN_ADC_MOSI       11
#define PIN_ADC_SCK        12
#define PIN_ADC_MISO       13
#define PIN_ADC_RESET      14  /* optional (else RESET command is used)             */
/* Optional sample-and-hold analog switch control (see docs/HARDWARE_AND_SAFETY.md section 7).
 * Leave UNASSIGNED until the S&H stage is built; set to 15 when it is. */
#define PIN_SH_GATE        PIN_UNASSIGNED
/* Optional: spare INPUT pin jumpered to the gate output (only if 'gate edges' stays 0). */
#define PIN_GATE_SENSE     PIN_UNASSIGNED

/* Output-enable polarity (level that ALLOWS pulses to reach the driver). */
#define OE_ACTIVE_LEVEL    1
#define OE_INACTIVE_LEVEL  0

/* Compile-time duplicate-pin guard: every assigned function must use a distinct GPIO. */
/* Optional pins set to PIN_UNASSIGNED (-1) are ignored. */
#define _NP_DUP(a, b) ((a) >= 0 && (a) == (b))
#if _NP_DUP(PIN_PULSE_GATE,PIN_OUTPUT_ENABLE)
#error "config.h: PIN_PULSE_GATE and PIN_OUTPUT_ENABLE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_PULSE_GATE,PIN_ESTOP_BUTTON)
#error "config.h: PIN_PULSE_GATE and PIN_ESTOP_BUTTON share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_PULSE_GATE,PIN_BUZZER)
#error "config.h: PIN_PULSE_GATE and PIN_BUZZER share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_PULSE_GATE,PIN_ADC_SYNC)
#error "config.h: PIN_PULSE_GATE and PIN_ADC_SYNC share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_PULSE_GATE,PIN_ADC_DRDY)
#error "config.h: PIN_PULSE_GATE and PIN_ADC_DRDY share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_PULSE_GATE,PIN_ADC_CS)
#error "config.h: PIN_PULSE_GATE and PIN_ADC_CS share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_PULSE_GATE,PIN_ADC_MOSI)
#error "config.h: PIN_PULSE_GATE and PIN_ADC_MOSI share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_PULSE_GATE,PIN_ADC_SCK)
#error "config.h: PIN_PULSE_GATE and PIN_ADC_SCK share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_PULSE_GATE,PIN_ADC_MISO)
#error "config.h: PIN_PULSE_GATE and PIN_ADC_MISO share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_PULSE_GATE,PIN_ADC_RESET)
#error "config.h: PIN_PULSE_GATE and PIN_ADC_RESET share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_PULSE_GATE,PIN_SH_GATE)
#error "config.h: PIN_PULSE_GATE and PIN_SH_GATE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_PULSE_GATE,PIN_GATE_SENSE)
#error "config.h: PIN_PULSE_GATE and PIN_GATE_SENSE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_OUTPUT_ENABLE,PIN_ESTOP_BUTTON)
#error "config.h: PIN_OUTPUT_ENABLE and PIN_ESTOP_BUTTON share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_OUTPUT_ENABLE,PIN_BUZZER)
#error "config.h: PIN_OUTPUT_ENABLE and PIN_BUZZER share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_OUTPUT_ENABLE,PIN_ADC_SYNC)
#error "config.h: PIN_OUTPUT_ENABLE and PIN_ADC_SYNC share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_OUTPUT_ENABLE,PIN_ADC_DRDY)
#error "config.h: PIN_OUTPUT_ENABLE and PIN_ADC_DRDY share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_OUTPUT_ENABLE,PIN_ADC_CS)
#error "config.h: PIN_OUTPUT_ENABLE and PIN_ADC_CS share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_OUTPUT_ENABLE,PIN_ADC_MOSI)
#error "config.h: PIN_OUTPUT_ENABLE and PIN_ADC_MOSI share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_OUTPUT_ENABLE,PIN_ADC_SCK)
#error "config.h: PIN_OUTPUT_ENABLE and PIN_ADC_SCK share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_OUTPUT_ENABLE,PIN_ADC_MISO)
#error "config.h: PIN_OUTPUT_ENABLE and PIN_ADC_MISO share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_OUTPUT_ENABLE,PIN_ADC_RESET)
#error "config.h: PIN_OUTPUT_ENABLE and PIN_ADC_RESET share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_OUTPUT_ENABLE,PIN_SH_GATE)
#error "config.h: PIN_OUTPUT_ENABLE and PIN_SH_GATE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_OUTPUT_ENABLE,PIN_GATE_SENSE)
#error "config.h: PIN_OUTPUT_ENABLE and PIN_GATE_SENSE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ESTOP_BUTTON,PIN_BUZZER)
#error "config.h: PIN_ESTOP_BUTTON and PIN_BUZZER share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ESTOP_BUTTON,PIN_ADC_SYNC)
#error "config.h: PIN_ESTOP_BUTTON and PIN_ADC_SYNC share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ESTOP_BUTTON,PIN_ADC_DRDY)
#error "config.h: PIN_ESTOP_BUTTON and PIN_ADC_DRDY share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ESTOP_BUTTON,PIN_ADC_CS)
#error "config.h: PIN_ESTOP_BUTTON and PIN_ADC_CS share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ESTOP_BUTTON,PIN_ADC_MOSI)
#error "config.h: PIN_ESTOP_BUTTON and PIN_ADC_MOSI share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ESTOP_BUTTON,PIN_ADC_SCK)
#error "config.h: PIN_ESTOP_BUTTON and PIN_ADC_SCK share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ESTOP_BUTTON,PIN_ADC_MISO)
#error "config.h: PIN_ESTOP_BUTTON and PIN_ADC_MISO share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ESTOP_BUTTON,PIN_ADC_RESET)
#error "config.h: PIN_ESTOP_BUTTON and PIN_ADC_RESET share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ESTOP_BUTTON,PIN_SH_GATE)
#error "config.h: PIN_ESTOP_BUTTON and PIN_SH_GATE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ESTOP_BUTTON,PIN_GATE_SENSE)
#error "config.h: PIN_ESTOP_BUTTON and PIN_GATE_SENSE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_BUZZER,PIN_ADC_SYNC)
#error "config.h: PIN_BUZZER and PIN_ADC_SYNC share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_BUZZER,PIN_ADC_DRDY)
#error "config.h: PIN_BUZZER and PIN_ADC_DRDY share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_BUZZER,PIN_ADC_CS)
#error "config.h: PIN_BUZZER and PIN_ADC_CS share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_BUZZER,PIN_ADC_MOSI)
#error "config.h: PIN_BUZZER and PIN_ADC_MOSI share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_BUZZER,PIN_ADC_SCK)
#error "config.h: PIN_BUZZER and PIN_ADC_SCK share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_BUZZER,PIN_ADC_MISO)
#error "config.h: PIN_BUZZER and PIN_ADC_MISO share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_BUZZER,PIN_ADC_RESET)
#error "config.h: PIN_BUZZER and PIN_ADC_RESET share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_BUZZER,PIN_SH_GATE)
#error "config.h: PIN_BUZZER and PIN_SH_GATE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_BUZZER,PIN_GATE_SENSE)
#error "config.h: PIN_BUZZER and PIN_GATE_SENSE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_SYNC,PIN_ADC_DRDY)
#error "config.h: PIN_ADC_SYNC and PIN_ADC_DRDY share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_SYNC,PIN_ADC_CS)
#error "config.h: PIN_ADC_SYNC and PIN_ADC_CS share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_SYNC,PIN_ADC_MOSI)
#error "config.h: PIN_ADC_SYNC and PIN_ADC_MOSI share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_SYNC,PIN_ADC_SCK)
#error "config.h: PIN_ADC_SYNC and PIN_ADC_SCK share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_SYNC,PIN_ADC_MISO)
#error "config.h: PIN_ADC_SYNC and PIN_ADC_MISO share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_SYNC,PIN_ADC_RESET)
#error "config.h: PIN_ADC_SYNC and PIN_ADC_RESET share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_SYNC,PIN_SH_GATE)
#error "config.h: PIN_ADC_SYNC and PIN_SH_GATE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_SYNC,PIN_GATE_SENSE)
#error "config.h: PIN_ADC_SYNC and PIN_GATE_SENSE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_DRDY,PIN_ADC_CS)
#error "config.h: PIN_ADC_DRDY and PIN_ADC_CS share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_DRDY,PIN_ADC_MOSI)
#error "config.h: PIN_ADC_DRDY and PIN_ADC_MOSI share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_DRDY,PIN_ADC_SCK)
#error "config.h: PIN_ADC_DRDY and PIN_ADC_SCK share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_DRDY,PIN_ADC_MISO)
#error "config.h: PIN_ADC_DRDY and PIN_ADC_MISO share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_DRDY,PIN_ADC_RESET)
#error "config.h: PIN_ADC_DRDY and PIN_ADC_RESET share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_DRDY,PIN_SH_GATE)
#error "config.h: PIN_ADC_DRDY and PIN_SH_GATE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_DRDY,PIN_GATE_SENSE)
#error "config.h: PIN_ADC_DRDY and PIN_GATE_SENSE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_CS,PIN_ADC_MOSI)
#error "config.h: PIN_ADC_CS and PIN_ADC_MOSI share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_CS,PIN_ADC_SCK)
#error "config.h: PIN_ADC_CS and PIN_ADC_SCK share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_CS,PIN_ADC_MISO)
#error "config.h: PIN_ADC_CS and PIN_ADC_MISO share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_CS,PIN_ADC_RESET)
#error "config.h: PIN_ADC_CS and PIN_ADC_RESET share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_CS,PIN_SH_GATE)
#error "config.h: PIN_ADC_CS and PIN_SH_GATE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_CS,PIN_GATE_SENSE)
#error "config.h: PIN_ADC_CS and PIN_GATE_SENSE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_MOSI,PIN_ADC_SCK)
#error "config.h: PIN_ADC_MOSI and PIN_ADC_SCK share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_MOSI,PIN_ADC_MISO)
#error "config.h: PIN_ADC_MOSI and PIN_ADC_MISO share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_MOSI,PIN_ADC_RESET)
#error "config.h: PIN_ADC_MOSI and PIN_ADC_RESET share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_MOSI,PIN_SH_GATE)
#error "config.h: PIN_ADC_MOSI and PIN_SH_GATE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_MOSI,PIN_GATE_SENSE)
#error "config.h: PIN_ADC_MOSI and PIN_GATE_SENSE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_SCK,PIN_ADC_MISO)
#error "config.h: PIN_ADC_SCK and PIN_ADC_MISO share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_SCK,PIN_ADC_RESET)
#error "config.h: PIN_ADC_SCK and PIN_ADC_RESET share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_SCK,PIN_SH_GATE)
#error "config.h: PIN_ADC_SCK and PIN_SH_GATE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_SCK,PIN_GATE_SENSE)
#error "config.h: PIN_ADC_SCK and PIN_GATE_SENSE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_MISO,PIN_ADC_RESET)
#error "config.h: PIN_ADC_MISO and PIN_ADC_RESET share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_MISO,PIN_SH_GATE)
#error "config.h: PIN_ADC_MISO and PIN_SH_GATE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_MISO,PIN_GATE_SENSE)
#error "config.h: PIN_ADC_MISO and PIN_GATE_SENSE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_RESET,PIN_SH_GATE)
#error "config.h: PIN_ADC_RESET and PIN_SH_GATE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_ADC_RESET,PIN_GATE_SENSE)
#error "config.h: PIN_ADC_RESET and PIN_GATE_SENSE share a GPIO; assign unique pins"
#endif
#if _NP_DUP(PIN_SH_GATE,PIN_GATE_SENSE)
#error "config.h: PIN_SH_GATE and PIN_GATE_SENSE share a GPIO; assign unique pins"
#endif

static inline bool cfgPulsePinsOk() {
  return PIN_PULSE_GATE >= 0 && PIN_OUTPUT_ENABLE >= 0 && PIN_PULSE_GATE != PIN_OUTPUT_ENABLE;
}
static inline bool cfgAdcPinsOk() {
  return PIN_ADC_SCK >= 0 && PIN_ADC_MISO >= 0 && PIN_ADC_MOSI >= 0 && PIN_ADC_CS >= 0 && PIN_ADC_DRDY >= 0;
}

/* ===================== Wi-Fi access point ==================================== */
#define WIFI_AP_SSID       "NOVA-PI-P2"
#define WIFI_AP_PASS       "change-me-123"   /* INSECURE DEFAULT: change before use on a shared/public site. */
#if defined(ARDUINO)
#warning "NOVA-PI: change WIFI_AP_PASS in include/config.h before field use"
#endif
#define WIFI_AP_CHANNEL    6

/* ===================== Safety interlocks ===================================== */
#define HEARTBEAT_TIMEOUT_MS  3000

/* ===================== Electrical model (drives the derived limits) ========== */
/* Supply and loop. Use 24.0f for 2x12V in series (the model will then refuse
 * the 55 V FET + clamp combination below - that is intended). */
#define SUPPLY_V            12.0f
#define SUPPLY_R_OHM        0.10f   /* ESTIMATE: 5 Ah lead-acid + fuse + wiring                 */
#define FET_RDS_ON_OHM      0.035f  /* ESTIMATE: IRLZ44N hot, driven from 12 V gate               */
#define SHUNT_OHM           0.05f   /* current-sense shunt in the source (0 if none)              */
/* Coil. ESTIMATE from tools/coil_calc.py for 60 cm diameter, ~16 turns of 1.0 mm wire (30 m). */
#define COIL_L_UH           435.0f
#define COIL_R_OHM          0.66f
#define COIL_L_UNCERT       1.6f    /* L assumed anywhere in [L/1.6, L*1.6]; worst case used each way */
#define COIL_SETTLE_US      25.0f   /* ring-down / damping time after the clamp stops conducting  */
/* Power stage limits. */
#define FET_VDS_MAX_V       55.0f   /* IRLZ44N = 55 V. A 400-600 V FET allows a 10x shorter flyback */
#define DRAIN_CLAMP_V       40.0f   /* ESTIMATE: drain voltage while the clamp conducts at peak current.
                                       Must be > SUPPLY_V + 5 and <= 0.85 * FET_VDS_MAX_V.          */
#define I_PEAK_MAX_A        4.0f    /* below the 5 A fuse                                          */
#define I_AVG_MAX_A         1.0f    /* mean battery current                                        */
#define CLAMP_P_MAX_W       3.0f    /* continuous power rating you actually fitted to the clamp   */

/* Absolute firmware floors/ceilings (the coil model is the real limiter). */
#define LIM_PULSE_MIN_US      10
#define LIM_PULSE_MAX_US      300
#define LIM_PERIOD_MIN_US     1000
#define LIM_PERIOD_MAX_US     10000
#define LIM_DUTY_MAX_PERCENT  10
#define LIM_DELAY_MIN_US      5
#define LIM_DELAY_MAX_US      1000
#define PERIOD_MARGIN_US      100

/* ===================== Multi-delay decay curve =============================== */
#define MAX_TAPS              4       /* delays sampled in rotation, one per pulse */

/* ===================== Sample-and-hold (only used when PIN_SH_GATE is set) ==== */
#define SH_TRACK_LEVEL        1       /* gate level that CLOSES the switch (tracking)   */
#define SH_WINDOW_US          6       /* tracking window; sample instant = its END      */
#define SH_SETTLE_US          2       /* hold-settle before the ADC restarts            */

/* ===================== Default timing ======================================== */
#define DEF_PULSE_US          100
#define DEF_PERIOD_US         2000
#define DEF_DELAY_US          130     /* >= derived minimum for the default coil model */
#define DEF_TAPS              4
#define DEF_TAP_STEP_US       40
#define DEF_SAMPLES_PER_PULSE 1

/* ===================== ADS1256 =============================================== */
#define ADC_CLKIN_HZ          7680000UL
#define ADC_SPI_HZ            1000000UL
#define ADC_VREF_VOLTS        2.5f
#define ADC_MUX_VALUE         0x08       /* AIN0-AINCOM. CONFIRM. */
#define ADC_BUFFER_ENABLE     false
#define DEF_DRATE             0xF0       /* 30000 SPS */
#define DEF_GAIN_CODE         0
#define ADC_TEST_DRATE        0x82
#define ADC_TEST_SAMPLES_PER_FRAME 5
#define ADC_SAT_CODE          8300000L
#define SYNC_PULSE_US         3
#define LATE_TOLERANCE_US     60
#define WINDOW_SAFETY_MARGIN_US 100
#define ADC_REQUIRE_DRDY_HIGH_AFTER_SYNC 1

/* ===================== Signal-processing defaults ============================ */
#define DEF_AVG_PULSES        16      /* attempts per TAP per frame */
#define DEF_EMA_ALPHA         0.2f
#define DEF_THRESHOLD_COUNTS  2000.0f
#define DEF_HYSTERESIS        0.6f
#define DEF_CONFIRM_FRAMES    2
#define DEF_DRIFT_ALPHA       0.002f
#define DEF_CAL_FRAMES        60
#define DEF_GB_MODE           0       /* 0 auto (on when measured), 1 on, 2 off */
#define DEF_DIST_EXP          6.0f    /* amplitude ~ 1/r^n; 6 = compact target far from coil */
