#pragma once
#include <Arduino.h>
#include "debug.h"
#include "pins.h"

// Divider values
static const float Rtop = 100000.0f;
static const float Rbot = 200000.0f;
static const float VBAT_CAL = 1.000f;

// Battery thresholds
static const float VBAT_FULL_TH = 4.00f;
static const float VBAT_MED_TH = 3.70f;
static const float VBAT_LOW_TH = 3.50f;

inline float readVccCalibrated()
{
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1); // 1.1V internal ref
    delay(2);
    ADCSRA |= _BV(ADSC);
    while (bit_is_set(ADCSRA, ADSC))
        ;
    uint16_t raw = ADC;
    return (1.1f * 1023.0f) / (float)raw; // Vcc in volts
}

inline float readBatteryVolts()
{
    (void)analogRead(VBAT_PIN);
    delay(3);
    long acc = 0;
    const int N = 8;
    for (int i = 0; i < N; i++)
    {
        acc += analogRead(VBAT_PIN);
        delay(3);
    }
    float adc = acc / float(N);
    float vcc = readVccCalibrated();
    float vA0 = (adc * vcc) / 1023.0f;
    float vb = vA0 * (Rtop + Rbot) / Rbot;
    float out = vb * VBAT_CAL;
    DBG_PRINT(F("[BAT] adc="));
    DBG_PRINT(adc);
    DBG_PRINT(F(" vcc="));
    DBG_PRINT(vcc, 3);
    DBG_PRINT(F(" vA0="));
    DBG_PRINT(vA0, 3);
    DBG_PRINT(F(" Vbat="));
    DBG_PRINT(out, 3);
    DBG_PRINTLN(F("V"));
    return out;
}

inline char batteryFlag(float v)
{
    if (v >= VBAT_FULL_TH)
        return 'F';
    if (v >= VBAT_MED_TH)
        return 'M';
    if (v >= VBAT_LOW_TH)
        return 'L';
    return '!';
}
