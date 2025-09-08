#include "interrupts.h"
#include "debug.h"

extern AppState g_app;
extern RTC_DS3231 rtc; // still provided by main

void interruptsInitCorePins()
{
    g_app.lastPinsD = PIND;                               // capture first
    PCMSK2 |= _BV(PCINT20) | _BV(PCINT21) | _BV(PCINT16); // D4, D5, D0(RX)
    PCICR |= _BV(PCIE2);
    interruptsClearWakeFlags();
}

void interruptsEnableTick(bool en)
{
    if (en)
        PCMSK2 |= _BV(PCINT21);
    else
        PCMSK2 &= ~_BV(PCINT21);
    g_app.tickWake = false;
}

void interruptsInitBacklightButton()
{
    g_app.lastPinsB = PINB;
    PCMSK0 |= _BV(PCINT2); // D10
    PCICR |= _BV(PCIE0);
    g_app.blButtonWake = false;
}

void interruptsMaskSwitch(bool mask)
{
    if (mask)
        PCMSK2 &= ~_BV(PCINT20); // disable D4
    else
        PCMSK2 |= _BV(PCINT20); // enable D4
}

// ------------ ISRs -------------
ISR(PCINT2_vect)
{
    uint8_t now = PIND;
    uint8_t changed = now ^ g_app.lastPinsD;
    g_app.lastPinsD = now;
    if (changed & _BV(PD4))
        g_app.switchWake = true; // slide
    if (changed & _BV(PD5))
    {
        if (g_app.currentMode == MODE_CLOCK)
        {
            if (now & _BV(PD5))
                g_app.tickWake = true; // rising
        }
        else
        {
            if (!(now & _BV(PD5)))
                g_app.tickWake = true; // falling
        }
    }
    if (changed & _BV(PD0))
        g_app.serialWake = true; // RX
}

ISR(PCINT0_vect)
{
    uint8_t now = PINB, ch = now ^ g_app.lastPinsB;
    g_app.lastPinsB = now;
    if (ch & _BV(PB2))
    {
        if ((now & _BV(PB2)) == 0)
            g_app.blButtonWake = true; // LOW press
    }
}
