#include "alarm_scheduler.h"
#include <RTClib.h>

extern RTC_DS3231 rtc; // from main
#include "app_state.h"
extern AppState g_app; // global state

#if ENABLE_ALARM_FAILSAFE
static uint32_t g_lastFireEpoch = 0; // last serviced alarm/sample epoch
#endif
static uint32_t g_nextEpoch = 0;   // next target on UPDATE_INTERVAL_SEC grid
static uint32_t g_elapsedBase = 0; // first on-grid epoch after entering mode

static void programAlarm(uint32_t epoch)
{
    if (!g_app.rtcAvailable)
        return;
    rtc.writeSqwPinMode(DS3231_OFF); // INT mode
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);
    DateTime dt((uint32_t)epoch);
    rtc.setAlarm1(dt, DS3231_A1_Date);
    DBG_PRINT(F("[ALRM] Next @ "));
    DBG_PRINT(dt.timestamp(DateTime::TIMESTAMP_TIME));
    DBG_PRINT(F(" ("));
    DBG_PRINT(epoch);
    DBG_PRINTLN(F(")"));
}

void hygroSchedulerInit(uint32_t startEpoch)
{
    if (!g_app.rtcAvailable)
        return;
    g_nextEpoch = startEpoch - (startEpoch % UPDATE_INTERVAL_SEC) + UPDATE_INTERVAL_SEC;
    if (g_nextEpoch <= startEpoch)
        g_nextEpoch += UPDATE_INTERVAL_SEC;
    g_elapsedBase = g_nextEpoch; // anchor
    programAlarm(g_nextEpoch);
#if ENABLE_ALARM_FAILSAFE
    g_lastFireEpoch = startEpoch; // treat as just fired now
#endif
}

bool hygroSchedulerShouldFire(uint32_t nowEpoch)
{
    if (!g_app.rtcAvailable)
        return false;
    if (g_nextEpoch == 0)
        return false;
    return rtc.alarmFired(1) || (nowEpoch >= g_nextEpoch);
}

void hygroSchedulerAdvanceAfterFire(uint32_t nowEpoch)
{
    if (!g_app.rtcAvailable)
        return;
    if (g_nextEpoch == 0)
        return;
    rtc.clearAlarm(1);
    // ensure next strictly in future on grid
    if (g_nextEpoch == 0)
        return;
    if (g_nextEpoch <= nowEpoch)
    {
        do
        {
            g_nextEpoch += UPDATE_INTERVAL_SEC;
        } while (g_nextEpoch <= nowEpoch);
    }
    programAlarm(g_nextEpoch);
}

void hygroSchedulerMarkSample(uint32_t nowEpoch)
{
#if ENABLE_ALARM_FAILSAFE
    g_lastFireEpoch = nowEpoch;
#endif
}

void hygroSchedulerSanity(uint32_t nowEpoch)
{
    if (!g_app.rtcAvailable)
        return;
    if (g_nextEpoch == 0)
        return;
    if (g_nextEpoch < nowEpoch)
        return; // let main treat as fired first
    uint32_t ahead = g_nextEpoch - nowEpoch;
    if (ahead > ALARM_MAX_AHEAD_SEC)
    {
        DBG_PRINTLN(F("[ALRM] Sanity realign"));
        // Realign to next grid from now
        g_nextEpoch = nowEpoch - (nowEpoch % UPDATE_INTERVAL_SEC) + UPDATE_INTERVAL_SEC;
        if (g_nextEpoch <= nowEpoch)
            g_nextEpoch += UPDATE_INTERVAL_SEC;
        g_elapsedBase = g_nextEpoch; // re-anchor after large jump
        programAlarm(g_nextEpoch);
    }
}

bool hygroSchedulerFailsafeCheck(uint32_t nowEpoch)
{
#if ENABLE_ALARM_FAILSAFE
    if (!g_app.rtcAvailable)
        return false;
    if (g_nextEpoch == 0)
        return false;
    if ((uint32_t)(nowEpoch - g_lastFireEpoch) > ALARM_FAILSAFE_SEC)
    {
        DBG_PRINTLN(F("[FS] Silence > window -> reschedule"));
        g_nextEpoch = nowEpoch - (nowEpoch % UPDATE_INTERVAL_SEC) + UPDATE_INTERVAL_SEC;
        if (g_nextEpoch <= nowEpoch)
            g_nextEpoch += UPDATE_INTERVAL_SEC;
        programAlarm(g_nextEpoch);
        g_lastFireEpoch = nowEpoch;
        return true;
    }
#endif
    return false;
}

uint32_t hygroSchedulerNextEpoch() { return g_nextEpoch; }
uint32_t hygroSchedulerBaseEpoch() { return g_elapsedBase; }
