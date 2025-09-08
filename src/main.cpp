// Core Arduino & libs
#include <Arduino.h>
#include <LiquidCrystal.h>
#include <DHT.h>
#include <RTClib.h>
#include <LowPower.h>
#include <avr/interrupt.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

// Modular headers
#include "config.h"
#include "pins.h"
#include "debug.h"
#include "battery.h"
#include "ui_format.h"
#include "backlight.h"
#include "alarm_scheduler.h"
#include "time_commands.h"
#include "app_state.h"
#include "interrupts.h"

// All configuration/constants in headers; this file orchestrates modes & main loop.

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 rtc;
AppState g_app; // global runtime state (see app_state.h)

// ---------- Forward declarations ----------
void updateHygroMode();
void updateClockMode();
void enterMode(DeviceMode m);
void sleepUntilAlarmOrSwitch();
bool sleepUntilNextOrSwitch(uint16_t remainSec, uint16_t *sleptSecOut);
void rtc_use_sqw_for_clock();
// (rtc_use_alarm_for_hygro now internal to scheduler)
void drainSerial(unsigned long ms = 250);

// ---------- Small helpers ----------
void lcdPrint16(const char *s)
{
  char b[17];
  uint8_t i = 0;
  for (; i < 16 && s[i]; ++i)
    b[i] = s[i];
  for (; i < 16; ++i)
    b[i] = ' ';
  b[16] = 0;
  lcd.print(b);
}
// Formatting / battery helpers live in dedicated modules.

// Minutes-only elapsed (dHH:MM, no seconds)
void formatElapsed(const TimeSpan &ts, char *out, size_t n)
{
  uint32_t mins = ts.totalseconds() / 60;
  snprintf(out, n, "%lud%02u:%02u",
           (unsigned long)(mins / (24u * 60u)),
           (uint8_t)((mins / 60u) % 24u),
           (uint8_t)(mins % 60u));
}

void formatElapsedMillis(unsigned long ms, char *out, size_t n)
{
  unsigned long m = (ms / 1000UL) / 60UL;
  snprintf(out, n, "%lud%02lu:%02lu",
           (unsigned long)(m / (24UL * 60UL)), (m / 60UL) % 24UL, m % 60UL);
}

DeviceMode readSwitchMode() { return (digitalRead(MODE_PIN) == LOW) ? MODE_HYGRO : MODE_CLOCK; }

// PCINT setup & ISRs: see interrupts.* ; currentSeconds() inline in app_state.h

// ---------- RTC serial helpers ----------
// (RTC print / parsing utilities now in time_commands module)

// (Time command parsing moved to time_commands module)
void drainSerial(unsigned long ms)
{
  unsigned long t0 = millis();
  while ((long)(millis() - t0) < (long)ms)
  {
    timeCommandsHandle();
    if (!Serial.available())
      delay(2);
  }
  g_app.serialWake = false;
}

// ---------- DS3231 helpers ----------
void rtc_use_sqw_for_clock()
{
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
  DBG_PRINTLN(F("[RTC] SQW=1Hz (Clock mode)"));
}

// (Direct rtc alarm programming moved to alarm_scheduler)

// ---------- Mode enter/update ----------
void enterMode(DeviceMode m)
{
  g_app.currentMode = m;
  // Suppress switch PCINT briefly to avoid chatter flooding after transition
  interruptsMaskSwitch(true);
  if (m == MODE_HYGRO)
  {
    DBG_PRINTLN(F("[MODE] Enter Hygrometer"));
    if (g_app.rtcAvailable)
    {
      g_app.modeStartRTC = rtc.now();
      uint32_t epoch = g_app.modeStartRTC.unixtime();
      hygroSchedulerInit(epoch);

      // Prepare UI & PCINT
      interruptsEnableTick(true); // D5 as INT (falling)
      g_app.lastPinsD = PIND;     // re-sync PCINT baseline

      lcd.setCursor(0, 0);
      lcdPrint16("Mode: Hygrometer");
      lcd.setCursor(0, 1);
      lcdPrint16("Init...");
      delay(50);

      // Immediate sample so we don't linger on "Init..."
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
#if ENABLE_ALARM_FAILSAFE
      // No RTC in this branch: failsafe logic is RTC-dependent and not used.
#endif
    }
  }
  else
  { // MODE_CLOCK
    DBG_PRINTLN(F("[MODE] Enter Clock"));
    if (g_app.rtcAvailable)
      rtc_use_sqw_for_clock();

    interruptsEnableTick(true); // D5 as SQW (rising)
    g_app.lastPinsD = PIND;     // re-sync PCINT baseline

    lcd.setCursor(0, 0);
    lcdPrint16("Mode: Clock     ");
    lcd.setCursor(0, 1);
    lcdPrint16(g_app.rtcAvailable ? "RTC OK" : "No RTC");
    delay(50);

    // brief keep-awake to ensure we see the first tick
    g_app.serialAwakeUntil = millis() + 1200;
  }
  // Re-enable after short suppression period tracked via timestamp
  g_app.lastModeEnterMs = millis(); // reuse existing timestamp
}

// Hygro: sleep until alarm (INT low) or user action
void sleepUntilAlarmOrSwitch()
{
  DBG_FLUSH();
  g_app.tickWake = false; // wait for fresh falling edge
  while (!g_app.tickWake && !g_app.switchWake && !g_app.blButtonWake && !g_app.serialWake)
  {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    if (g_app.tickWake || g_app.switchWake || g_app.blButtonWake || g_app.serialWake)
      break;
    if (g_app.rtcAvailable)
    {
      uint32_t nowEpoch = rtc.now().unixtime();
      if (hygroSchedulerFailsafeCheck(nowEpoch))
      {
        updateHygroMode();
        hygroSchedulerMarkSample(nowEpoch);
        break;
      }
    }
  }
  if (g_app.tickWake)
    DBG_PRINTLN(F("[WAKE] Alarm/INT"));
  else if (g_app.switchWake)
    DBG_PRINTLN(F("[WAKE] Slide"));
  else if (g_app.blButtonWake)
    DBG_PRINTLN(F("[WAKE] Backlight btn"));
  else if (g_app.serialWake)
    DBG_PRINTLN(F("[WAKE] Serial RX"));
}

// Fallback when no RTC: chunked WDT sleep
bool sleepUntilNextOrSwitch(uint16_t remainSec, uint16_t *sleptSecOut)
{
  uint16_t orig = remainSec;
  g_app.tickWake = false;
  while (remainSec > 0)
  {
    uint8_t c = (remainSec >= 8) ? 8 : (remainSec >= 4) ? 4
                                   : (remainSec >= 2)   ? 2
                                                        : 1;
    LowPower.powerDown((c == 8) ? SLEEP_8S : (c == 4) ? SLEEP_4S
                                         : (c == 2)   ? SLEEP_2S
                                                      : SLEEP_1S,
                       ADC_OFF, BOD_OFF);
    if (g_app.switchWake || g_app.blButtonWake || g_app.tickWake || g_app.serialWake)
      break;
    remainSec -= c;
  }
  uint16_t slept = orig - remainSec;
  if (sleptSecOut)
    *sleptSecOut = slept;
  g_app.tickWake = false;
  return (remainSec == 0);
}

// Clock: sleep until 1 Hz tick / slide / button / serial
void sleepUntilTickOrSwitch()
{
  DBG_FLUSH();
  g_app.tickWake = false;
  while (!g_app.tickWake && !g_app.switchWake && !g_app.blButtonWake && !g_app.serialWake)
  {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
  if (g_app.tickWake)
    DBG_PRINTLN(F("[WAKE] SQW 1Hz"));
  g_app.tickWake = false;
}

// 12-hour Clock UI with AM/PM
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
    DateTime dummy((uint32_t)0); // not used when haveRTC=false
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

  // --- anchored minutes (RTC path) ---
  char ebuf[12];
  if (g_app.rtcAvailable)
  {
    uint32_t nowEpoch = rtc.now().unixtime();
    uint32_t base = hygroSchedulerBaseEpoch();
    uint32_t secs = (nowEpoch > base) ? (nowEpoch - base) : 0;
    TimeSpan el(secs);
    formatElapsed(el, ebuf, sizeof(ebuf)); // minutes-only
  }
  else
  {
    // Fallback (no RTC): same as before (minutes from millis)
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

// ---------- setup/loop ----------
void setup()
{
  pinMode(DHT_PWR, OUTPUT);
  digitalWrite(DHT_PWR, LOW);
  pinMode(VBAT_PIN, INPUT);
  pinMode(MODE_PIN, INPUT_PULLUP);
  pinMode(SQW_PIN, INPUT_PULLUP);

  pinMode(BL_BUTTON_PIN, INPUT_PULLUP); // button

  DBG_BEGIN(115200);
  DBG_PRINTLN(F("[BOOT]"));

  analogReference(DEFAULT); // we measure Vcc dynamically in readVcc()

  lcd.begin(16, 2);
  delay(80);
  lcd.setCursor(0, 0);
  lcdPrint16("DIY Hygrometer ");
  lcd.setCursor(0, 1);
  lcdPrint16("LCD+DHT22+RTC  ");
  delay(800);

  if (rtc.begin())
  {
    g_app.rtcAvailable = true;
    if (rtc.lostPower())
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
    g_app.startTimeRTC = rtc.now();
    g_app.modeStartRTC = g_app.startTimeRTC;
    Serial.println(F("Clock mode serial cmds: RD | CT[=±offset] | T=YYYY-MM-DD HH:MM:SS | U=<unix_epoch>"));
  }

  g_app.startMillis = millis();
  g_app.modeStartMillis = millis();
  g_app.modeSleepSecondsAccum = 0;
  g_app.softSeconds = 0;
  g_app.sysSeconds = 0;
  // Scheduler state resets on first hygroSchedulerInit

  interruptsInitCorePins();
  interruptsInitBacklightButton();
  backlightInit();

  enterMode(readSwitchMode());
  g_app.lastStableMode = g_app.currentMode;
  g_app.lastModeEnterMs = millis();
  g_app.lastModeReadMs = g_app.lastModeEnterMs;
}

void loop()
{
  // Mode change via slide switch (debounced + re-entry guard)
  DeviceMode rawMode = readSwitchMode();
  unsigned long nowMsLoop = millis();
  if (rawMode != g_app.lastStableMode)
  {
    // provisional change; require stability for MODE_DEBOUNCE_MS
    if ((nowMsLoop - g_app.lastModeReadMs) >= MODE_DEBOUNCE_MS)
    {
      g_app.lastStableMode = rawMode;
      g_app.lastModeReadMs = nowMsLoop;
      g_app.switchWake = true; // treat as a switch wake event
    }
  }
  else
  {
    g_app.lastModeReadMs = nowMsLoop; // stable reading refreshes timestamp
  }

  if (g_app.switchWake && (g_app.lastStableMode != g_app.currentMode))
  {
    if ((nowMsLoop - g_app.lastModeEnterMs) >= MODE_REENTRY_GUARD_MS)
    {
      DBG_PRINTLN(F("[MODE] Debounced switch change"));
      appClearWakeFlags();
      enterMode(g_app.lastStableMode);
      // enterMode sets lastModeEnterMs; keep for guards
    }
    else
    {
      // Guard period: ignore rapid flips
      g_app.switchWake = false;
    }
  }
  else if (g_app.switchWake)
  {
    // Switch wake but no logical mode change: clear to avoid repeated wake spam
    g_app.switchWake = false;
  }

  // Release switch PCINT mask after suppression window
  if ((nowMsLoop - g_app.lastModeEnterMs) >= MODE_SWITCH_SUPPRESS_MS)
    interruptsMaskSwitch(false);

  // Backlight button debounce + turn on
  if (g_app.blButtonWake || digitalRead(BL_BUTTON_PIN) == LOW)
  {
    unsigned long nowMs = millis();
    if (nowMs - g_app.blLastHandledMs > BL_DEBOUNCE_MS)
    {
      g_app.blLastHandledMs = nowMs;
      g_app.blButtonWake = false;
      backlightOn();
    }
    else
      g_app.blButtonWake = false;
  }

  // Keep-awake extension if backlight or serial activity
  if (backlightIsActive())
    g_app.serialAwakeUntil = millis() + 1500; // slightly longer
  if (g_app.serialWake || Serial.available())
  {
    g_app.serialAwakeUntil = millis() + 1200;
    g_app.serialWake = false;
    drainSerial(300);
  }
  if (g_app.currentMode == MODE_CLOCK)
  {
    if ((long)(g_app.serialAwakeUntil - millis()) > 0)
      timeCommandsHandle();
    updateClockMode();

    if ((long)(g_app.serialAwakeUntil - millis()) > 0)
    {
      drainSerial(60);
      return;
    }

    if (g_app.rtcAvailable)
    {
      sleepUntilTickOrSwitch();
    }
    else
    {
      DBG_FLUSH();
      LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
      g_app.softSeconds++;
      g_app.sysSeconds++;
    }
  }
  else
  {
    // Hygrometer mode
    if (g_app.rtcAvailable)
    {
      uint32_t nowEpoch = rtc.now().unixtime();
      bool fired = hygroSchedulerShouldFire(nowEpoch);

      if (fired)
      {
        hygroSchedulerAdvanceAfterFire(nowEpoch);

        // Take the sample
        updateHygroMode();
        hygroSchedulerMarkSample(nowEpoch);
      }

      // Only perform sanity adjustment AFTER we service any fired alarm.
      hygroSchedulerSanity(nowEpoch);
      if (hygroSchedulerFailsafeCheck(nowEpoch))
      {
        updateHygroMode();
        hygroSchedulerMarkSample(nowEpoch);
      }

      // Allow serial commands during awake window
      if ((long)(g_app.serialAwakeUntil - millis()) > 0)
      {
        timeCommandsHandle();
        delay(5);
        return;
      }

      sleepUntilAlarmOrSwitch();
    }
    else
    {
      // Fallback (no RTC): legacy WDT schedule
      unsigned long nowMs = millis();
      if (g_app.lastHygroUpdateMillis == 0 || (nowMs - g_app.lastHygroUpdateMillis) >= (UPDATE_INTERVAL_SEC * 1000UL))
      {
        updateHygroMode();
        g_app.lastHygroUpdateMillis = millis();
      }
      if ((long)(g_app.serialAwakeUntil - millis()) > 0)
      {
        delay(5);
        return;
      }
      unsigned long elapsed = (millis() - g_app.lastHygroUpdateMillis) / 1000UL;
      uint16_t remain = (elapsed >= UPDATE_INTERVAL_SEC) ? 0 : (UPDATE_INTERVAL_SEC - elapsed);
      if (remain > 0)
      {
        uint16_t slept = 0;
        bool timedOut = sleepUntilNextOrSwitch(remain, &slept);
        g_app.modeSleepSecondsAccum += slept;
        g_app.sysSeconds += slept;
        if (timedOut)
          g_app.lastHygroUpdateMillis = 0;
      }
    }
  }
}
