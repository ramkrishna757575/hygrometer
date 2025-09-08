#pragma once
#include <Arduino.h>
#include <RTClib.h>
#include "config.h"
#include "debug.h"

// Hygrometer alarm scheduling / failsafe module (DS3231 based)
// All state private to implementation. Functions are no-ops when RTC absent.

void hygroSchedulerInit(uint32_t startEpoch);           // initialize next alarm grid & anchor elapsed base
bool hygroSchedulerShouldFire(uint32_t nowEpoch);       // true if DS3231 alarm fired or time >= next epoch
void hygroSchedulerAdvanceAfterFire(uint32_t nowEpoch); // advance next epoch and reprogram alarm (call after fire detection, before sampling)
void hygroSchedulerMarkSample(uint32_t nowEpoch);       // mark that a sample was just taken (updates failsafe bookkeeping)
void hygroSchedulerSanity(uint32_t nowEpoch);           // realign if alarm scheduled too far ahead
bool hygroSchedulerFailsafeCheck(uint32_t nowEpoch);    // reschedule if silence > failsafe window; returns true if rescheduled

uint32_t hygroSchedulerNextEpoch(); // current next target epoch (0 if uninitialized / no RTC)
uint32_t hygroSchedulerBaseEpoch(); // anchored elapsed base epoch (0 if not set)
