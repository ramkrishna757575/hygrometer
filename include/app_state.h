#pragma once
#include <Arduino.h>
#include <RTClib.h>

// Forward enum (defined in main or separate header if split later)
#ifndef DEVICE_MODE_ENUM_DEFINED
#define DEVICE_MODE_ENUM_DEFINED
enum DeviceMode : uint8_t
{
    MODE_CLOCK = 0,
    MODE_HYGRO = 1
};
#endif

struct AppState
{
    // RTC / mode
    bool rtcAvailable = false;
    DeviceMode currentMode = MODE_HYGRO;
    DateTime startTimeRTC;
    DateTime modeStartRTC;

    // Millis-based fallbacks / counters
    unsigned long startMillis = 0;
    unsigned long modeStartMillis = 0;
    unsigned long lastHygroUpdateMillis = 0;
    unsigned long modeSleepSecondsAccum = 0;
    unsigned long softSeconds = 0; // clock mode w/o RTC
    unsigned long sysSeconds = 0;  // coarse seconds for backlight when no RTC

    // PCINT / wake flags (volatile for ISR access)
    volatile bool switchWake = false;
    volatile bool tickWake = false;
    volatile bool serialWake = false;
    volatile uint8_t lastPinsD = 0;

    // Backlight button PCINT (B port)
    volatile bool blButtonWake = false;
    volatile uint8_t lastPinsB = 0;

    // Backlight / serial keep-awake bookkeeping
    unsigned long blLastHandledMs = 0;
    unsigned long serialAwakeUntil = 0;

    // Mode switch debounce bookkeeping
    unsigned long lastModeReadMs = 0;
    unsigned long lastModeEnterMs = 0;
    DeviceMode lastStableMode = MODE_HYGRO;
};

extern AppState g_app; // defined in main.cpp

// Inline helpers centralizing time and wake flag operations
extern RTC_DS3231 rtc; // provided by main.cpp

inline uint32_t currentSeconds()
{
    return g_app.rtcAvailable ? rtc.now().unixtime() : g_app.sysSeconds;
}

inline void appClearWakeFlags()
{
    g_app.switchWake = g_app.tickWake = g_app.serialWake = false;
}
