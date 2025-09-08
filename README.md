# Hygrometer / Clock Firmware

Low-power Arduino Pro Mini (ATmega328P 16MHz) firmware providing a dual-mode device:

- Clock mode: 12-hour time display with battery voltage and optional serial time setting commands.
- Hygrometer mode: Periodic temperature & humidity sampling (DHT22) with elapsed runtime and battery display.

## Hardware

- Arduino Pro Mini (5V / 16MHz)
- DS3231 RTC (1Hz SQW + Alarm1 used)
- DHT22 sensor (powered from a switched GPIO to save energy)
- 16x2 HD44780 LCD (LiquidCrystal)
- Backlight MOSFET or transistor on BACKLIGHT_PIN
- Slide switch selects mode (Clock / Hygro)
- Backlight push button (PCINT)

## Key Modules

| Module              | Purpose                                                               |
| ------------------- | --------------------------------------------------------------------- |
| `config.h`          | Timing constants, feature toggles                                     |
| `pins.h`            | All pin assignments                                                   |
| `debug.h`           | Debug print macros (compiled out when disabled)                       |
| `battery.h`         | Vcc + battery voltage measurement & classification                    |
| `ui_format.*`       | Pure string builders for LCD lines (no I/O side effects)              |
| `backlight.*`       | Backlight state, duration timing, auto-off logic                      |
| `alarm_scheduler.*` | DS3231 alarm grid scheduling + sanity + failsafe                      |
| `time_commands.*`   | Serial RTC command parsing (RD / CT / T= / U=)                        |
| `app_state.h`       | Central consolidated runtime state & inline helpers                   |
| `interrupts.*`      | PCINT setup & ISR handlers (slide switch, tick, backlight, serial RX) |

## Central State (`AppState`)

All mutable runtime data (mode, debounce timers, wake flags, millis counters, RTC availability, etc.) lives in the single global `g_app` instance defined in `main.cpp` and declared in `app_state.h`.

Important fields:

- `rtcAvailable` – true when DS3231 initialized OK.
- `currentMode` / `lastStableMode` – debounced logical mode + active mode.
- `switchWake`, `tickWake`, `serialWake`, `blButtonWake` – ISR-set volatile wake flags.
- `softSeconds`, `sysSeconds` – software-maintained seconds (no RTC path).
- `lastModeReadMs`, `lastModeEnterMs` – debounce + re-entry guard for mode switch.
- `lastHygroUpdateMillis`, `modeSleepSecondsAccum` – hygrometer update timing when no RTC.

Inline helpers:

- `currentSeconds()` – unified seconds source (RTC if present else `sysSeconds`).
- `appClearWakeFlags()` – clears slide / tick / serial wake flags atomically.

## Scheduling & Failsafe

`alarm_scheduler` aligns hygrometer samples to a fixed second grid (`UPDATE_INTERVAL_SEC`). It also:

- Realigns if an alarm is programmed suspiciously far ahead (`ALARM_MAX_AHEAD_SEC`).
- Triggers a failsafe reschedule if no sample/alarm activity occurs within `ALARM_FAILSAFE_SEC` (optional macro).

Without an RTC the firmware emulates timing with chunked LowPower WDT sleeps (1/2/4/8s) accumulating into `modeSleepSecondsAccum`.

## Backlight

`backlightOn()` records start seconds (using `currentSeconds()`) and `backlightMaintain()` auto turns it off after `BACKLIGHT_DURATION_SEC`. The user button or serial activity can extend awake time.

## Serial Time Commands (Clock Mode)

While the device is awake in Clock mode (during a short keep-awake window after activity) the following commands are accepted:

- `RD` / `R` / `D` – Read current RTC timestamp.
- `CT[=±offset]` – Set RTC to compile time (with optional seconds or HH:MM:SS offset, sign supported).
- `T=YYYY-MM-DD HH:MM:SS` – Set explicit timestamp.
- `U=<unix_epoch>` – Set from UNIX epoch.

## Power Behaviors

- DHT sensor is powered only during readings.
- Device sleeps in 8s slices while waiting for RTC alarms (or WDT fallback).
- Wake sources: slide switch (mode change), DS3231 SQW/alarm, serial RX, backlight button.

## Building

Build / Upload (PlatformIO):

```
pio run
pio run --target upload
```

## Extending

Add new features by creating a new module instead of expanding `main.cpp`. Add any new mutable globals into `AppState` to keep cross-module dependencies explicit. Interrupt sources should be added in `interrupts.*` so wake flag logic stays in one place.

## License

Released under the MIT License. See `LICENSE` for full text.
