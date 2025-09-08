#pragma once
#include <Arduino.h>
#include "config.h"

#if ENABLE_SERIAL_DEBUG || ENABLE_SERIAL_RTC_CMDS
#define DBG_BEGIN(...) Serial.begin(__VA_ARGS__)
#define DBG_PRINT(...)                 \
    do                                 \
    {                                  \
        if (ENABLE_SERIAL_DEBUG)       \
            Serial.print(__VA_ARGS__); \
    } while (0)
#define DBG_PRINTLN(...)                 \
    do                                   \
    {                                    \
        if (ENABLE_SERIAL_DEBUG)         \
            Serial.println(__VA_ARGS__); \
    } while (0)
#define DBG_FLUSH()              \
    do                           \
    {                            \
        if (ENABLE_SERIAL_DEBUG) \
            Serial.flush();      \
    } while (0)
#else
#define DBG_BEGIN(...)
#define DBG_PRINT(...)
#define DBG_PRINTLN(...)
#define DBG_FLUSH()
#endif
