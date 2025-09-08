# Hygrometer (DHT22 + DS3231 + 16x2 LCD) Wiring

Target hardware reflected by current firmware (`pins.h`).

## Components

- Arduino Pro Mini 328P (5V, 16MHz)
- DS3231 RTC module (INT/SQW pin available)
- DHT22 sensor (AM2302) with optional separate pull-up (usually on breakout)
- 16x2 HD44780 LCD (parallel 4-bit)
- NPN transistor or MOSFET for LCD backlight (optional if direct drive within current limits)
- Backlight momentary push button
- Slide switch (mode select: Clock / Hygro)
- Li-ion cell + step-up/USB supply (battery monitoring on A0 via divider)

## Pin Mapping (Firmware)

| Function            | Arduino Pin | Direction | Notes                                   |
| ------------------- | ----------- | --------- | --------------------------------------- |
| LCD RS              | D12         | OUT       | HD44780 RS                              |
| LCD EN              | D11         | OUT       | Enable                                  |
| LCD D4              | D6          | OUT       | Data bit 4                              |
| LCD D5              | D7          | OUT       | Data bit 5                              |
| LCD D6              | D8          | OUT       | Data bit 6                              |
| LCD D7              | D9          | OUT       | Data bit 7                              |
| Backlight (PWM OK)  | D13         | OUT       | Drives transistor/MOSFET gate           |
| Mode Switch         | D4          | IN PULLUP | Slide to GND = Hygro mode               |
| DS3231 INT/SQW      | D5          | IN PULLUP | 1Hz SQW (Clock) / Alarm (Hygro) PCINT   |
| DHT22 Data          | D2          | IN        | Single-wire data                        |
| DHT22 Power Control | D3          | OUT       | Powers sensor only during read          |
| Backlight Button    | D10 (PB2)   | IN PULLUP | PCINT wake                              |
| Battery Sense       | A0          | IN        | Divider to measure Vbat                 |
| I2C SDA             | A4          | I2C       | RTC SDA (internal pull-ups on breakout) |
| I2C SCL             | A5          | I2C       | RTC SCL                                 |
| Serial RX           | D0          | IN        | Wake on incoming char                   |
| Serial TX           | D1          | OUT       | Debug / commands                        |

## Power & Battery Sensing

- Battery feeds a resistor divider into A0 (scale chosen so Vbat max < Vref). Adjust formula in `battery.h` if resistor values differ.
- DHT22 powered via D3 to reduce sleep current (set LOW when idle).
- Backlight auto-off after `BACKLIGHT_DURATION_SEC` controlled in firmware.

## Mode Switch

Slide switch ties D4 to GND for Hygrometer mode; released (pull-up) = Clock mode. Firmware adds debounce + brief interrupt mask after mode change to suppress chatter.

## RTC (DS3231)

- Connect SDA → A4, SCL → A5, VCC → 5V, GND → GND.
- INT/SQW → D5. Firmware switches between 1Hz square wave (clock mode) and Alarm1 interrupts (hygro scheduling).

## Backlight Control

- BACKLIGHT_PIN (D13) drives transistor base/gate (with base resistor if BJT, e.g. 1k). Emitter to GND, collector to LCD LED cathode; LCD LED anode to +5V via series resistor.
- Momentary button on D10 to GND (internal pull-up enabled). Debounced in firmware.

## DHT22 Wiring

| DHT22 Pin                                                                                                                | Connect | Notes                              |
| ------------------------------------------------------------------------------------------------------------------------ | ------- | ---------------------------------- |
| VCC                                                                                                                      | 5V      | Can also run at 3.3V               |
| DATA                                                                                                                     | D2      | 10k pull-up to VCC often on module |
| GND                                                                                                                      | GND     | Common ground                      |
| (Optional) VCC is switched by D3 so physical VCC may be from D3 line if using breakout without separate VCC pin control. |

If using a raw sensor (no PCB), add 10k between DATA and VCC, optionally a 100nF decoupling capacitor near sensor.

## ASCII Overview

```
     +-------------------+              +------------------+
     |   Arduino Pro     |              |    DS3231 RTC    |
     |                   | A4 <-------> | SDA              |
     |               D5 <-------------  | INT/SQW          |
     |                   | A5 <-------> | SCL              |
     |                   |              +------------------+
  Mode<-|D4                 |
 Switch |                   | D3 ---> (DHT Power)
     |                   | D2 <---- DHT22 DATA
     |                   |
 Backlt |D13 ---> Transistor--> LCD LED
 Button |D10 ---[BTN]--- GND
     |                   | A0 <--- Battery Divider
     |                   | D12/D11/D6/D7/D8/D9 -> LCD
     |                   |
     +-------------------+
```

## Programming (FTDI)

| FTDI | Pro Mini |
| ---- | -------- |
| VCC  | VCC      |
| GND  | GND      |
| TXD  | RXI (D0) |
| RXD  | TXO (D1) |
| DTR  | DTR      |

## Notes / Tips

1. Keep DHT22 leads short or twisted to reduce noise.
2. If slide switch still bounces, increase `MODE_SWITCH_SUPPRESS_MS` or `MODE_DEBOUNCE_MS` in `config.h`.
3. For lower power, remove power LED and regulator from Pro Mini, and ensure DS3231 uses a low-drift coin cell.
4. Consider adding a Schottky diode or fuse if powering from battery + USB simultaneously.
5. Verify battery divider ratio and adjust calculation constants before trusting voltage readout.

## Expected Sleep Currents (Approx.)

| State               | Approx Current (est.)         |
| ------------------- | ----------------------------- |
| Deep sleep (RTC on) | < 1 mA (depends on modules)   |
| Backlight active    | +10–40 mA (LCD LED dependent) |
| DHT read burst      | +1–2 mA over baseline         |

Fine-tune by measuring with a USB power meter or inline ammeter.

## Battery Divider Design

Current firmware constants (`battery.h`):

```
Rtop = 100kΩ
Rbot = 200kΩ
VBAT = V(A0) * (Rtop + Rbot) / Rbot = V(A0) * 1.5
```

Why this ratio:

1. Keeps divider load low (~16.7 µA @ 5V) to reduce idle drain.
2. Multiplier 1.5 keeps ADC node (< 2.9V) even when Li‑ion peaks at 4.2–4.25V.
3. High enough resistance to minimize wasted current yet low enough for stable ADC sampling without very long settling (we still throw away first sample + average N=8).

Alternative sets (pick one; update `Rtop`, `Rbot`, and recalibrate `VBAT_CAL` if needed):

| Goal                 | Rtop | Rbot | Factor ( (Rtop+Rbot)/Rbot ) | Divider Current @4.2V |
| -------------------- | ---- | ---- | --------------------------- | --------------------- |
| Lower noise          | 68k  | 150k | 1.453                       | ~18.6 µA              |
| Ultra low standby    | 220k | 470k | 1.468                       | ~6.3 µA               |
| Wider margin (5V in) | 150k | 150k | 2.000                       | ~14.0 µA              |

After changing values, take several voltage measurements with a DMM and adjust `VBAT_CAL` so computed value matches (VBAT_CAL = Vtrue / Vmeasured_firmware_average).

Accuracy tips:

- Take readings after a short backlight-off period so Vcc is stable.
- Perform calibration near mid battery voltage (~3.8V) to minimize scaling error across range.
- If using extremely high resistances (>1M total), add a small capacitor (e.g. 10–47 nF) from A0 to GND to stabilize sampling.

## Optional Backlight Driver Schematic

```
                +5V
                 |
                [Rlim]  (LCD LED series resistor, e.g. 47–150Ω depending on module)
                 |
        LCD LED Anode
                 |
        LCD LED Cathode
                 |
                Collector (NPN) / Drain (N-MOSFET)
                        |\
                        | \  NPN (e.g. 2N3904) OR Logic N-MOSFET (e.g. AO3400)
                        |  \
                        |   \  Emitter / Source → GND
                        |
     D13 ---[1k]--- Base (if BJT)
                        |
                    (Gate direct if MOSFET; add 100Ω series optional)
                        |
                    GND

Push button (backlight): D10 →——[BTN]——→ GND (internal pull-up enabled)
```

PWM dimming: D13 can be analogWrite()'d later if desired; currently firmware uses simple ON duration.

## Full System ASCII (Expanded)

```
    Li-ion + Protection
                |
         +--+-----> (Boost/5V Reg) ----+-------------------+ 5V
         |                             |                   |
        GND                           Vcc                 LCD Vcc
         |                             |                   |
         |                             |             +-----------+
         |                             |             | 16x2 LCD  |
         |                             |   RS  D12 --|4         |
         |                             |   EN  D11 --|6         |
         |                             |   D4   D6 --|11        |
         |                             |   D5   D7 --|12        |
         |                             |   D6   D8 --|13        |
         |                             |   D7   D9 --|14        |
         |                             |             |LED+A  Vcc|
         |                             |             |LED-K  |  |
         |                             |              +-----------+
         |                             |                   |
         |                           D13 -> Driver -> LED-K (through Rlim)
         |                             |
    Divider (Rtop over Rbot)         |
    VBAT ---/\/\/\---+---/\/\/\--- GND
                                     |
                                    A0

    DHT22:  D3 (switched Vcc) -> VCC   D2 -> DATA   GND -> GND
    DS3231: Vcc->5V GND->GND SDA->A4 SCL->A5 INT/SQW->D5
    Mode Switch: D4 ->---/ switch /--- GND
    Backlight Button: D10 ->---[BTN]--- GND
```

## Power Optimization Guide

Hardware mods (descending impact):

1. Remove power LED & regulator on Pro Mini (saves ~2–5 mA continuously).
2. Use MOSFET for backlight and choose higher series resistor; target lowest acceptable brightness.
3. Increase divider resistors (see table) to cut microamp leakage.
4. Ensure DHT22 fully unpowered when not sampling (pin D3 LOW). Verify no phantom path via DATA line; if so add series 1k on DATA.
5. Avoid leaving DS3231 32kHz output enabled (not used here). Only INT/SQW needed.
6. If using square wave in clock mode and power is critical, consider using only alarms + software second count to reduce continuous wake rate (trade accuracy).
7. Shield long sensor wires; noise can cause retries (extra awake time).

Firmware tweaks:

- Reduce `BACKLIGHT_DURATION_SEC` if user interaction brief.
- Increase `UPDATE_INTERVAL_SEC` if slower environmental change acceptable.
- Disable serial debug (`ENABLE_SERIAL_DEBUG 0`) once stable.
- Consider dynamic lowering of sample rate when battery flag is 'L'.

Advanced (optional):

- Enable BOD disable during sleep (requires bootloader or manual register tweak) to save additional hundreds of microamps.
- Calibrate internal oscillator (if ever running lower frequency variant) for tighter timing if you remove 1Hz SQW usage.

## Future Ideas

- Store rolling humidity averages and only wake LCD / backlight on meaningful delta.
- Implement battery voltage smoothing (exponential filter) to reduce flag flicker.
