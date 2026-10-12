#pragma once
#include <Arduino.h>
#include "settings.h"
#include "signal_proc.h"

enum AcqMode : uint8_t { MODE_IDLE = 0, MODE_SIM = 1, MODE_ADC_TEST = 2, MODE_DETECT = 3 };

struct AcqStatus {
  AcqMode mode; AcqMode req; bool pulsing; bool adcOk; uint8_t adcId;
  uint32_t adcReads, adcTimeouts, late, syncAnom, missed, noEdge, edges, frames, satFrames, invalid;
  char err[64];
};

void acq_begin();
bool acq_start(AcqMode m, const char** err);      /* async; errors appear in status.err */
void acq_stop(const char* reason);                /* IMMEDIATE hardware stop in caller's context */
void acq_calibrate();
void acq_setReference(float cm);                  /* current detected signal := target at 'cm' */
void acq_resetBaseline();
bool acq_getFrame(Frame& f);
void acq_status(AcqStatus& s);
void acq_getSettings(Settings& s);
bool acq_setSettings(const Settings& n, const char** err);
void acq_heartbeat();
void acq_loopService();                           /* call from loop(): heartbeat + physical STOP */
