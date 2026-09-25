# Hardware

emini Home 0.6.2 supports one device: the **ZECTRIX NOTE4C Devkit** with the
four-colour display.

| Part | Details |
| --- | --- |
| Display | 400 × 300 e-paper, black, white, yellow and red pigments |
| Chip | ESP32-S3 with native USB (tested unit: revision 0.2) |
| Memory | 16 MiB flash, 8 MiB octal PSRAM |
| Controls | Up (GPIO39) and Down (GPIO18) on the right edge, OK / BOOT (GPIO0) on the face |
| Notes | Down shares its line with the board's power key, so its level is less clean than the other two; a status LED sits on GPIO3 (Home keeps it off since 0.6.1) and a reset pinhole on RST/EN |
| Connectivity | 2.4 GHz Wi-Fi |

The monochrome NOTE4 uses a different display and needs its own firmware.
Other ZECTRIX models and other ESP32-S3 boards are not supported either. A USB
chip ID only tells you the board has an ESP32-S3, and the preflight check in
the [installation guide](INSTALL.md) cannot tell a NOTE4 from a NOTE4C. Check
that the device in front of you is a NOTE4C with the four-colour display
before you install.

## Pins

Pin mapping follows the manufacturer's
[NOTE4C quick start](https://wiki.zectrix.com/en/hardware/note4c/quick-start)
and the reference firmware by LazyYoun at commit
[`51812e4`](https://github.com/LazyYoun/youn-ink-fourcolor-firmware/tree/51812e4ab3fa80ba7a5a5a274635ca2cf3901a25).

| Function | GPIO |
| --- | --- |
| Display SPI clock, data, chip select | 12, 13, 11 |
| Display data/command, reset, busy | 10, 9, 8 |
| Display power | 6 |
| Buttons: Up, Down, OK / BOOT | 39, 18, 0 |
| Power latch | 17 |
| Native USB D−, D+ | 19, 20 |
| Battery voltage | 4 (ADC1 channel 3, 2:1 divider) |
| Charger: charging (active low), full (active high) | 2, 1 |

## Display

A frame is 30,000 bytes: 2 bits per pixel, 4 pixels per byte, most
significant bits first. Index 0 is black, 1 is white (paper), 2 is yellow and
3 is red. Home sends only full refreshes. On the tested unit, powered over
USB, a full change of the image took about 25 seconds. Home skips the refresh
when the new frame is identical to the one on screen.

Dither patterns never place yellow or red in steps finer than 2 pixels, while
black and paper patterns can use single pixels.

Colours in screenshots and on the website are an approximation of the
pigments, not a colorimeter measurement.

## Display driver and time zones

The display driver in `firmware/main/home_panel.c` adapts the pin map and the
four-colour command sequence from LazyYoun's reference firmware at commit
`51812e4` (`config.h` and `custom_lcd_display.cc`), under the MIT notice kept
at the top of that file. Home's changes: it sends only full refreshes, checks
that the panel really starts each refresh, waits for the BUSY line for at most
120 seconds, and stops with a fault instead of retrying when the panel does not
respond. The battery percentage curve in `firmware/main/home_battery.c` comes
from the same firmware.

The time zone table in `firmware/main/generated/home_zones.c` is compiled from
the [IANA Time Zone Database](https://www.iana.org/time-zones), release 2026c,
which is in the public domain.

## Battery indicator

Home reads the battery voltage ten times every 30 seconds and averages the
calibrated readings. Readings outside 2,800–4,350 mV count as unknown.
Charger signals must stay stable for a second before the panel shows them.

While not charging, the panel estimates a percentage from the voltage with the
curve used by the reference firmware, clamped to 0–100 %:
`(-V*V + 9016*V - 19189000) / 10000`, where `V` is in millivolts. It is a
rough estimate, not a fuel gauge, and it is hidden while charging.

Battery life on the tested unit. Drawing costs little: over four and a half
days of everyday use, 715 pictures kept the panel busy about 4.6 % of the time.
The current goes into keeping the radio and the chip awake.

| Release | Behaviour | Radio on | A full charge lasts |
| --- | --- | --- | --- |
| 0.5.2 and earlier | Wi-Fi connected, no sleep at all | all the time | a day and a half to two days, estimated |
| 0.6, Open | chip sleeps between events, Wi-Fi connected and waking for every third beacon | all the time | about a week, estimated |
| 0.6, Breath (default) | Wi-Fi off between downloads; a press of OK opens the panel for five minutes | 15 to 55 seconds an hour overnight, measured | two to four weeks, estimated |

The 0.5.2 figure comes from the device's own week of battery readings: two full
days on battery used about 50 and 55 percentage points of charge. It is an
extrapolation, not a measured discharge. The 0.6 figures are estimates too. The
radio time in Breath is measured, from the device's hourly log over a 13-hour run
on battery; a later 44-hour stretch averaged 42 seconds an hour. A full discharge
on 0.6 has not been done yet. The device keeps that log for a week (see
[privacy](PRIVACY.md)), so a week on battery is enough to replace the estimates with
a measurement.

In Open most of the cost is the radio. In its default mode the Wi-Fi driver woke the
chip about seven times a second and held it awake about a third of the time,
whatever the rest of the firmware did. Set to wake for every third beacon, it wakes
about three times a second and leaves the chip free to sleep about 80 % of the time.
The price is a little latency for the frames the access point holds back: on the
tested unit the panel still answers in well under a second and the `.local` name
still resolves. Breath removes that cost between downloads by turning the radio off.

On a cable Home holds power management locks and never sleeps, so flashing and
the USB maintenance protocol behave exactly as they did before.

## Flash layout

The factory layout on the tested unit, and what emini Home adds:

| Name | Offset | Size | Written by Home |
| --- | --- | --- | --- |
| bootloader | `0x0` | 32 KiB | no |
| partition table | `0x8000` | 3 KiB | yes, adds `home_nvs` |
| nvs (factory) | `0x9000` | 16 KiB | no |
| otadata | `0xD000` | 8 KiB | no |
| phy_init | `0xF000` | 4 KiB | no |
| **home_nvs** | `0x10000` | 64 KiB | yes, created in an unused gap |
| ota_0 | `0x20000` | 4,032 KiB | yes, application |
| ota_1 | `0x410000` | 4,032 KiB | no |
| assets | `0x800000` | 8 MiB | no |

Home keeps its settings, Wi-Fi details, paired browsers and last good data in
`home_nvs`. See [installation](INSTALL.md) for the exact write commands.
