#pragma once

// ---- Feature / Debug Toggles ----
#define ENABLE_SERIAL_RTC_CMDS 1
#define ENABLE_SERIAL_DEBUG 0 // Set 0 to save power once done debugging

// ---- Timing ----
#define UPDATE_INTERVAL_SEC 30      // Hygro sample period (s)
#define DHT_SETTLE_MS 1800          // DHT power-up settle
#define BACKLIGHT_DURATION_SEC 10UL // Backlight auto-off
#define BL_DEBOUNCE_MS 150UL        // Backlight button debounce

// ---- Alarm / Failsafe ----
#define ENABLE_ALARM_FAILSAFE 1
#define ALARM_FAILSAFE_SEC 120 // Silence window before forced reschedule
#define ALARM_MAX_AHEAD_SEC 90 // Max future offset allowed for next sample

// ---- Mode Switch Debounce ----
#define MODE_DEBOUNCE_MS 80UL
#define MODE_REENTRY_GUARD_MS 300UL
#define MODE_SWITCH_SUPPRESS_MS 120UL // mask switch PCINT after mode change

// ---- Sensor Config ----
#define DHTTYPE DHT22
