#include "ui_format.h"
#include "debug.h"

static void fmtFloatLocal(float v, uint8_t w, uint8_t p, char *out, size_t n)
{
    dtostrf(v, w, p, out);
    out[n - 1] = 0;
}

void buildClockLines(bool haveRTC,
                     const DateTime &now,
                     unsigned long softSeconds,
                     float vbat,
                     char *line1, size_t l1n,
                     char *line2, size_t l2n)
{
    int hour12;
    bool pm = false;
    uint8_t mm, ss;
    uint8_t day = 0, mon = 0, yy = 0;
    if (haveRTC)
    {
        int h = now.hour();
        mm = now.minute();
        ss = now.second();
        day = now.day();
        mon = now.month();
        yy = now.year() % 100;
        if (h == 0)
            hour12 = 12;
        else if (h == 12)
        {
            hour12 = 12;
            pm = true;
        }
        else if (h > 12)
        {
            hour12 = h - 12;
            pm = true;
        }
        else
            hour12 = h;
    }
    else
    {
        unsigned long s = softSeconds;
        unsigned long h24 = (s / 3600UL) % 24UL;
        mm = (s / 60UL) % 60UL;
        ss = s % 60UL;
        int h = (int)h24;
        if (h == 0)
            hour12 = 12;
        else if (h == 12)
        {
            hour12 = 12;
            pm = true;
        }
        else if (h > 12)
        {
            hour12 = h - 12;
            pm = true;
        }
        else
            hour12 = h;
    }
    char vb[8];
    dtostrf(vbat, 1, 2, vb);
    snprintf(line1, l1n, "%02d:%02u:%02u %s  Batt", hour12, mm, ss, pm ? "PM" : "AM");
    if (haveRTC)
    {
        snprintf(line2, l2n, "%02u/%02u/%02u %sV %c", day, mon, yy, vb, batteryFlag(vbat));
    }
    else
    {
        snprintf(line2, l2n, "No RTC   %sV %c", vb, batteryFlag(vbat));
    }
}

void buildHygroLine1(float tc, float rh, char *line1, size_t n)
{
    if (!isnan(rh) && !isnan(tc))
    {
        char tbuf[8];
        fmtFloatLocal(tc, 4, 1, tbuf, sizeof(tbuf));
        snprintf(line1, n, "%4s%cC  RH %2d%%", tbuf, DEGREE_CHAR, (int)round(rh));
    }
    else
    {
        snprintf(line1, n, "SENSOR ERROR");
    }
}

void buildHygroLine2(const char *elapsed, char rtcFlag,
                     float vbat, char batFlag,
                     char *line2, size_t n)
{
    int elen = strlen(elapsed);
    char vbStr[10];
    if (elen <= 7)
    {
        fmtFloatLocal(vbat, 4, 2, vbStr, sizeof(vbStr));
        snprintf(line2, n, "E%s%c %sV%c", elapsed, rtcFlag, vbStr, batFlag);
    }
    else if (elen == 8)
    {
        fmtFloatLocal(vbat, 3, 1, vbStr, sizeof(vbStr));
        snprintf(line2, n, "E%s%c %sV%c", elapsed, rtcFlag, vbStr, batFlag);
    }
    else
    {
        fmtFloatLocal(vbat, 1, 0, vbStr, sizeof(vbStr));
        snprintf(line2, n, "E%s%c%sV%c", elapsed, rtcFlag, vbStr, batFlag);
    }
}

// Minutes-only elapsed (dHH:MM, no seconds) from TimeSpan
void formatElapsed(const TimeSpan &ts, char *out, size_t n)
{
    uint32_t mins = ts.totalseconds() / 60;
    snprintf(out, n, "%lud%02u:%02u",
             (unsigned long)(mins / (24u * 60u)),
             (uint8_t)((mins / 60u) % 24u),
             (uint8_t)(mins % 60u));
}

// Millis-based fallback (no RTC)
void formatElapsedMillis(unsigned long ms, char *out, size_t n)
{
    unsigned long m = (ms / 1000UL) / 60UL;
    snprintf(out, n, "%lud%02lu:%02lu",
             (unsigned long)(m / (24UL * 60UL)), (m / 60UL) % 24UL, m % 60UL);
}
