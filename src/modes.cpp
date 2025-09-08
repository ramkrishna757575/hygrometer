#include <Arduino.h>
#include <LowPower.h>
#include "modes.h"
#include "display_utils.h"
#include "debug.h"
#include "alarm_scheduler.h"
#include "ui_format.h"
#include "battery.h"
#include "backlight.h"
#include "interrupts.h"

// Local helper: set SQW for clock mode
static void rtc_use_sqw_for_clock()
{
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);
    rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
    DBG_PRINTLN(F("[RTC] SQW=1Hz (Clock mode)"));
}

DeviceMode readSwitchMode()
{
    return (digitalRead(MODE_PIN) == LOW) ? MODE_HYGRO : MODE_CLOCK;
}

void enterMode(DeviceMode m)
{
    g_app.currentMode = m;
    interruptsMaskSwitch(true); // suppress chatter after transition
    if (m == MODE_HYGRO)
    {
        DBG_PRINTLN(F("[MODE] Enter Hygrometer"));
        if (g_app.rtcAvailable)
        {
            g_app.modeStartRTC = rtc.now();
            uint32_t epoch = g_app.modeStartRTC.unixtime();
            hygroSchedulerInit(epoch);

            interruptsEnableTick(true); // D5 as INT (falling)
            g_app.lastPinsD = PIND;

            lcd.setCursor(0, 0);
            lcdPrint16("Mode: Hygrometer");
            lcd.setCursor(0, 1);
            lcdPrint16("Init...");
            delay(50);

            updateHygroMode();
            hygroSchedulerMarkSample(epoch);
        }
        else
        {
            g_app.modeStartMillis = millis();
            g_app.lastHygroUpdateMillis = 0;
            g_app.modeSleepSecondsAccum = 0;
            interruptsEnableTick(true);
            g_app.lastPinsD = PIND;
            lcd.setCursor(0, 0);
            lcdPrint16("Mode: Hygrometer");
            lcd.setCursor(0, 1);
            lcdPrint16("Init...");
            delay(50);
            updateHygroMode();
        }
    }
    else
    {
        DBG_PRINTLN(F("[MODE] Enter Clock"));
        if (g_app.rtcAvailable)
            rtc_use_sqw_for_clock();
        interruptsEnableTick(true); // D5 as SQW
        g_app.lastPinsD = PIND;
        lcd.setCursor(0, 0);
        lcdPrint16("Mode: Clock     ");
        lcd.setCursor(0, 1);
        lcdPrint16(g_app.rtcAvailable ? "RTC OK" : "No RTC");
        delay(50);
        g_app.serialAwakeUntil = millis() + 1200;
    }
    g_app.lastModeEnterMs = millis();
}

void updateClockMode()
{
    static int8_t lastSecRTC = -1;
    static unsigned long lastSoftSec = (unsigned long)-1;
    if (g_app.rtcAvailable)
    {
        DateTime now = rtc.now();
        if (now.second() == lastSecRTC)
            return;
        lastSecRTC = now.second();
        float vbat = readBatteryVolts();
        char l1[17], l2[17];
        buildClockLines(true, now, 0, vbat, l1, sizeof(l1), l2, sizeof(l2));
        lcd.setCursor(0, 0);
        lcdPrint16(l1);
        lcd.setCursor(0, 1);
        lcdPrint16(l2);
    }
    else
    {
        if (g_app.softSeconds == lastSoftSec)
            return;
        lastSoftSec = g_app.softSeconds;
        float vbat = readBatteryVolts();
        char l1[17], l2[17];
        DateTime dummy((uint32_t)0);
        buildClockLines(false, dummy, g_app.softSeconds, vbat, l1, sizeof(l1), l2, sizeof(l2));
        lcd.setCursor(0, 0);
        lcdPrint16(l1);
        lcd.setCursor(0, 1);
        lcdPrint16(l2);
    }
    backlightMaintain(currentSeconds());
}

void updateHygroMode()
{
    DBG_PRINTLN(F("[HYGRO] Power DHT..."));
    digitalWrite(DHT_PWR, HIGH);
    delay(DHT_SETTLE_MS);
    dht.begin();
    float rh = dht.readHumidity();
    float tc = dht.readTemperature();
    if (isnan(rh) || isnan(tc))
    {
        DBG_PRINTLN(F("[HYGRO] Retry read"));
        delay(400);
        rh = dht.readHumidity();
        tc = dht.readTemperature();
    }
    digitalWrite(DHT_PWR, LOW);
    float vbat = readBatteryVolts();
    DBG_PRINT(F("[HYGRO] T="));
    DBG_PRINT(tc, 1);
    DBG_PRINT(F("C  RH="));
    DBG_PRINT(rh, 1);
    DBG_PRINT(F("%  Vbat="));
    DBG_PRINT(vbat, 3);
    DBG_PRINTLN(F("V"));
    char l1[17];
    buildHygroLine1(tc, rh, l1, sizeof(l1));
    lcd.setCursor(0, 0);
    lcdPrint16(l1);
    char ebuf[12];
    if (g_app.rtcAvailable)
    {
        uint32_t nowEpoch = rtc.now().unixtime();
        uint32_t base = hygroSchedulerBaseEpoch();
        uint32_t secs = (nowEpoch > base) ? (nowEpoch - base) : 0;
        TimeSpan el(secs);
        formatElapsed(el, ebuf, sizeof(ebuf));
    }
    else
    {
        unsigned long awakeMs = millis() - g_app.modeStartMillis;
        unsigned long elapsedMs = awakeMs + (g_app.modeSleepSecondsAccum * 1000UL);
        formatElapsedMillis(elapsedMs, ebuf, sizeof(ebuf));
    }
    char l2[17];
    char rtcFlag = g_app.rtcAvailable ? 'R' : 'T';
    char batFlag = batteryFlag(vbat);
    buildHygroLine2(ebuf, rtcFlag, vbat, batFlag, l2, sizeof(l2));
    lcd.setCursor(0, 1);
    lcdPrint16(l2);
    DBG_PRINT(F("[HYGRO] LCD L2: "));
    DBG_PRINTLN(l2);
    DBG_PRINT(F("[HYGRO] Elapsed="));
    DBG_PRINT(ebuf);
    DBG_PRINT('(');
    DBG_PRINT(rtcFlag);
    DBG_PRINT(F(")  V="));
    int elen = strlen(ebuf);
    DBG_PRINT(vbat, (elen <= 7) ? 2 : ((elen == 8) ? 1 : 0));
    DBG_PRINT(F("V  Flag="));
    DBG_PRINT(batFlag);
    DBG_PRINTLN();
    backlightMaintain(currentSeconds());
}
