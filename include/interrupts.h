#pragma once
#include <Arduino.h>
#include <avr/interrupt.h>
#include "app_state.h"
#include "pins.h"

// Initializes PCINT for D4 (mode switch), D5 (tick/alarm), D0 (serial RX)
void interruptsInitCorePins();
// Enable/disable tick pin change (D5) depending on mode behavior.
void interruptsEnableTick(bool en);
// Initializes PCINT for backlight button (D10 / PB2)
void interruptsInitBacklightButton();
// Temporarily mask/unmask the slide switch PCINT (D4) to suppress chatter.
void interruptsMaskSwitch(bool mask);
// Clear core wake flags (switch/tick/serial)
inline void interruptsClearWakeFlags() { appClearWakeFlags(); }
