#pragma once
#include <Arduino.h>

// Handle incoming serial RTC/time-setting commands.
// Safe to call in any mode; commands only act if RTC present.
void timeCommandsHandle();
