#include "time_commands.h"
#include <RTClib.h>
#include <ctype.h>
#include "debug.h"

extern RTC_DS3231 rtc; // from main.cpp
#include "app_state.h"
extern AppState g_app;

static void printRTC()
{
    if (!g_app.rtcAvailable)
    {
        Serial.println(F("[RTC] not available"));
        return;
    }
    DateTime now = rtc.now();
    Serial.print(F("[RTC] "));
    Serial.println(now.timestamp(DateTime::TIMESTAMP_FULL));
}

static bool parseYMDHMS(const char *s, DateTime &out)
{
    if (!s || strlen(s) < 19)
        return false;
    if (s[4] != '-' || s[7] != '-' || s[10] != ' ' || s[13] != ':' || s[16] != ':')
        return false;
    int yr = atoi(s), mo = atoi(s + 5), dy = atoi(s + 8), hh = atoi(s + 11), mm = atoi(s + 14), ss = atoi(s + 17);
    if (yr < 2000 || mo < 1 || mo > 12 || dy < 1 || dy > 31 || hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59)
        return false;
    out = DateTime(yr, mo, dy, hh, mm, ss);
    return true;
}

static bool parseOffsetSeconds(const char *s, long &outSecs)
{
    while (*s == ' ')
        s++;
    int sign = 1;
    if (*s == '+' || *s == '-')
    {
        sign = (*s == '+') ? 1 : -1;
        s++;
    }
    while (*s == ' ')
        s++;
    const char *c = s;
    bool hasColon = false;
    while (*c)
    {
        if (*c == ':')
        {
            hasColon = true;
            break;
        }
        if (*c == ' ')
            break;
        c++;
    }
    if (hasColon)
    {
        int hh = 0, mm = 0, ss = 0;
        if (!isdigit(*s))
            return false;
        while (isdigit(*s))
        {
            hh = hh * 10 + (*s - '0');
            s++;
        }
        if (*s++ != ':')
            return false;
        if (!isdigit(*s))
            return false;
        while (isdigit(*s))
        {
            mm = mm * 10 + (*s - '0');
            s++;
        }
        if (*s++ != ':')
            return false;
        if (!isdigit(*s))
            return false;
        while (isdigit(*s))
        {
            ss = ss * 10 + (*s - '0');
            s++;
        }
        while (*s == ' ')
            s++;
        if (*s != '\0')
            return false;
        outSecs = sign * ((long)hh * 3600L + (long)mm * 60L + (long)ss);
        return true;
    }
    else
    {
        char *endp;
        long v = strtol(s, &endp, 10);
        while (*endp == ' ')
            endp++;
        if (*endp != '\0')
            return false;
        outSecs = sign * v;
        return true;
    }
}

static void processTimeCommand(const char *line)
{
    if (!line || !g_app.rtcAvailable)
    {
        Serial.println(F("[RTC] not available or bad command"));
        return;
    }
    if (!strcmp(line, "R") || !strcmp(line, "D") || !strcmp(line, "RD"))
    {
        printRTC();
        return;
    }
    if (!strncmp(line, "CT", 2) || !strncmp(line, "C", 1))
    {
        const char *p = line + (line[0] == 'C' && line[1] != 'T' ? 1 : 2);
        while (*p == ' ')
            p++;
        if (*p == '=')
        {
            p++;
            while (*p == ' ')
                p++;
        }
        DateTime base = DateTime(F(__DATE__), F(__TIME__));
        long off = 0;
        bool hasOffset = (*p != '\0');
        if (hasOffset && !parseOffsetSeconds(p, off))
        {
            Serial.println(F("[ERR] CT offset: seconds or HH:MM:SS"));
            return;
        }
        long newEpoch = (long)base.unixtime() + off;
        if (newEpoch < 0)
            newEpoch = 0;
        rtc.adjust(DateTime((uint32_t)newEpoch));
        rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
        Serial.print(F("[RTC] set to compile time"));
        if (hasOffset)
        {
            Serial.print(F(" + "));
            Serial.print(off);
            Serial.print(F("s"));
        }
        Serial.println();
        printRTC();
        return;
    }
    if (!strncmp(line, "T=", 2))
    {
        DateTime dt;
        const char *s = line + 2;
        while (*s == ' ')
            s++;
        if (parseYMDHMS(s, dt))
        {
            rtc.adjust(dt);
            rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
            Serial.println(F("[RTC] set to given timestamp"));
            printRTC();
        }
        else
            Serial.println(F("[ERR] Use T=YYYY-MM-DD HH:MM:SS"));
        return;
    }
    if (!strncmp(line, "U=", 2))
    {
        const char *s = line + 2;
        while (*s == ' ')
            s++;
        unsigned long epoch = strtoul(s, nullptr, 10);
        rtc.adjust(DateTime(epoch));
        rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
        Serial.println(F("[RTC] set from UNIX epoch"));
        printRTC();
        return;
    }
    Serial.println(F("Commands: RD | CT[=±offset] | T=YYYY-MM-DD HH:MM:SS | U=<unix_epoch>"));
    Serial.println(F("CT offset examples: CT=+10  CT -45  CT=+01:02:03"));
}

void timeCommandsHandle()
{
    static char buf[64];
    static uint8_t idx = 0;
    while (Serial.available())
    {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n')
        {
            buf[idx] = 0;
            idx = 0;
            char *p = buf;
            while (*p == ' ')
                p++;
            int n = strlen(p);
            while (n > 0 && p[n - 1] == ' ')
                p[--n] = 0;
            if (*p)
            {
                for (char *q = p; *q && *q != ' ' && *q != '='; ++q)
                    *q = toupper(*q);
                processTimeCommand(p);
            }
        }
        else if (idx < sizeof(buf) - 1)
            buf[idx++] = c;
    }
}
