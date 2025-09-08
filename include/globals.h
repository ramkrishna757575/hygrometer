// Extern declarations for global hardware objects and app state.
#pragma once
#include <LiquidCrystal.h>
#include <DHT.h>
#include <RTClib.h>
#include "app_state.h"

extern LiquidCrystal lcd;
extern DHT dht;
extern RTC_DS3231 rtc;
extern AppState g_app;
