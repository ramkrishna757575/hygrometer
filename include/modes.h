#pragma once
#include "config.h"
#include "pins.h"
#include "globals.h"

// Device mode entry (handles RTC setup, scheduler init, UI banners)
void enterMode(DeviceMode m);

// Mode periodic update functions
void updateClockMode();
void updateHygroMode();

// Read debounced raw switch level (no debounce logic, just pin)
DeviceMode readSwitchMode();
