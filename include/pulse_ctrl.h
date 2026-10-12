#pragma once
#include <Arduino.h>

struct PulseTiming { uint32_t pulse_us, period_us, delay_us; uint8_t taps; uint16_t dstep_us; };

void        pulse_begin();                                  /* call FIRST in setup(): forces safe state */
const char* pulse_validate(const PulseTiming& t, uint32_t adcWindowUs); /* nullptr = ok, else reason */
bool        pulse_start(const PulseTiming& t, TaskHandle_t notifyTask, const char** err);
void        pulse_stop();                                   /* IMMEDIATE. Safe from any task. */
bool        pulse_running();
bool        pulse_estopTriggered();                         /* true once after physical STOP */
uint32_t    pulse_edgeCount();
int64_t     pulse_lastEdgeUs();                             /* esp_timer time of last gate falling edge */
bool        pulse_lastEdge(int64_t* t_us, uint32_t* index);  /* consistent (time, edge number) pair; edge numbers start at 1 */
void        pulse_setDelayUs(uint32_t delay_us);             /* HW SYNC (+S&H) delay for the NEXT period; latched at timer overflow */
