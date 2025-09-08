#include <Arduino.h>
#include "globals.h"

// Print and pad/truncate to exactly 16 chars
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
