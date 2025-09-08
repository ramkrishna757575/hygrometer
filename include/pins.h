#pragma once

// LCD pins (HD44780 4-bit)
#define LCD_RS 12
#define LCD_EN 11
#define LCD_D4 6
#define LCD_D5 7
#define LCD_D6 8
#define LCD_D7 9

// Sensors & IO
#define DHTPIN 2
#define DHT_PWR 3
#define VBAT_PIN A0
#define MODE_PIN 4 // slide to GND = Hygro
#define SQW_PIN 5  // DS3231 INT/SQW wired here
#define BACKLIGHT_PIN 13
#define BL_BUTTON_PIN 10
#define DEGREE_CHAR 223
