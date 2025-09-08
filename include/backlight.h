#pragma once
#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "debug.h"

void backlightInit();
void backlightOn();
void backlightOff();
void backlightMaintain(uint32_t nowSeconds); // auto-off check
bool backlightIsActive();
uint32_t backlightStartSeconds();
