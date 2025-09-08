#include "backlight.h"
#include "app_state.h" // for inline currentSeconds()

static bool g_active = false;
static uint32_t g_startSec = 0;

// rtcAvailable no longer needed directly; using g_app if required in future
// (kept currentSeconds extern for time base)
// currentSeconds inline is available via app_state.h

void backlightInit()
{
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, LOW);
    g_active = false;
    g_startSec = 0;
}

void backlightOn()
{
    digitalWrite(BACKLIGHT_PIN, HIGH);
    g_active = true;
    g_startSec = currentSeconds();
    DBG_PRINTLN(F("[BL] ON"));
}

void backlightOff()
{
    digitalWrite(BACKLIGHT_PIN, LOW);
    if (g_active)
        DBG_PRINTLN(F("[BL] OFF"));
    g_active = false;
}

void backlightMaintain(uint32_t nowSeconds)
{
    if (!g_active)
        return;
    if ((int32_t)(nowSeconds - g_startSec) >= (int32_t)BACKLIGHT_DURATION_SEC)
        backlightOff();
}

bool backlightIsActive() { return g_active; }
uint32_t backlightStartSeconds() { return g_startSec; }
