#pragma once
#include <Arduino.h>
#include <RTClib.h>
#include "config.h"
#include "pins.h"
#include "battery.h"

// Build 16-char (or shorter) lines; caller pads via lcdPrint16.
// Clock mode: if haveRTC true, use DateTime; else softSeconds for fallback.
void buildClockLines(bool haveRTC,
                     const DateTime &now,
                     unsigned long softSeconds,
                     float vbat,
                     char *line1, size_t l1n,
                     char *line2, size_t l2n);

// Hygrometer line 1: temperature + humidity or error.
void buildHygroLine1(float tc, float rh, char *line1, size_t n);

// Hygrometer line 2 built from elapsed string (already computed),
// rtcFlag ('R' or 'T'), battery voltage + flag.
void buildHygroLine2(const char *elapsed, char rtcFlag,
                     float vbat, char batFlag,
                     char *line2, size_t n);

// Elapsed time helpers (minutes granularity: dHH:MM). These were in main.cpp.
void formatElapsed(const TimeSpan &ts, char *out, size_t n);          // RTC-based (TimeSpan)
void formatElapsedMillis(unsigned long ms, char *out, size_t n);       // Millis-based fallback
