# emini Home

**A calm, four-colour poster of your day for the ZECTRIX NOTE4C e-paper devkit.**

`0.4.2` · [emini.ink](https://emini.ink/home/) · tested on one NOTE4C · ESP-IDF v6.0 · MIT

<p align="center">
  <img src="docs/images/note4c-photo.webp" width="720" alt="A ZECTRIX NOTE4C on a fridge door running emini Home 0.4.0. The Weather screen in the Print composition shows 15° in Czaplinek, 14–17 °C over 24 hours, cloud cover, and rain from 21:00 to 23:00.">
</p>
<p align="center"><sub>Photo of a NOTE4C running 0.4.0 · Weather in the Print composition</sub></p>

emini Home puts the weather forecast, one headline and a note in your own
words on the NOTE4C's 400 × 300 display, using its black, white, red and
yellow pigments. You set it up in your phone's browser, without an app or an
account. After that the device does the rest on its own, and no emini.ink
server sits in between.

[Install](docs/INSTALL.md) · [Phone panel](docs/PANEL.md) · [Release 0.4.2](https://github.com/fiedoruk/emini-home/releases/tag/v0.4.2) · [Website](https://emini.ink/home/) · [Hardware report](https://github.com/fiedoruk/emini-home/issues/new?template=hardware-report.yml)

> [!WARNING]
> Version 0.4.2 has been installed and tested on one NOTE4C. Read the
> [status](#status) before you install. The preflight check in the
> installation guide tells you to stop if your device's boot data differs
> from the tested one. It cannot tell a NOTE4 from a NOTE4C, and only the
> four-colour NOTE4C is supported.

## One forecast, three compositions

<p align="center">
  <img src="docs/images/epaper-weather-rhythm.png" width="400" alt="Rhythm composition: a sample forecast for Warsaw with a dithered temperature curve for the next hours">
  <img src="docs/images/epaper-weather-atlas.png" width="400" alt="Atlas composition: the same sample forecast with a large sun and cloud on the left half">
</p>
<p align="center"><sub>Rhythm and Atlas, drawn on a computer by the 0.4.2 renderer from sample data. The photo at the top shows Print.</sub></p>

Rhythm draws the coming hours as a curve, and Atlas gives the sky half of the
page. Each screen can keep one composition, or use **In turn** and move on to
the next one each time it comes back to the display. The weather screen shows
the range for the next 24 hours under the temperature and a short line about precipitation,
such as “Rain from 18:00”.

There is no grey on this display, so the renderer mixes the four pigments in
ordered dither patterns to draw warmth, light and cloud. Coloured patterns are
never finer than 2 pixels, while black and paper patterns can still use single
pixels.

## One headline and your note

<p align="center">
  <img src="docs/images/epaper-news-print.png" width="400" alt="The News screen: one headline from a news feed">
  <img src="docs/images/epaper-note-rhythm.png" width="400" alt="The Your note screen: a personal note in large type above a dithered band">
</p>
<p align="center"><sub>News and Your note, drawn from sample data</sub></p>

News comes from BBC World by default, or from a public RSS or Atom feed of your
choice, as long as it is served over HTTPS. The note is yours: a reminder, a
line from a friend, a few words to keep in view.

Headlines and notes in Simplified Chinese are drawn with Noto Sans CJK glyphs
(the 3 755 most common characters plus punctuation); lines break between
characters, so a Chinese feed such as a news site's RSS works as it is.

## Set it up from your phone

On first start the display shows a setup screen. Join the **emini.ink** Wi-Fi
network it shows, open `http://192.168.4.1`, type the pairing code from the
display, and move the device onto your home network. Then search for your town,
pick your screens and their compositions, and choose one screen, a day rhythm
or a rotation, with quiet hours for the night.

<p align="center">
  <img src="docs/images/epaper-setup.png" width="400" alt="The setup screen on the display: QR codes for the setup Wi-Fi and the panel address, and a pairing code">
  <img src="docs/images/panel-pair-crop.webp" width="195" alt="Pairing page in the phone browser: the six-digit code and the Connect this phone button">
  <img src="docs/images/panel-location-search-crop.webp" width="195" alt="Town search on the Weather page: Warsaw typed in, and matching places with their region and country">
</p>
<p align="center"><sub>Panel screenshots were taken in a browser on a computer, with sample data.</sub></p>

The [phone panel guide](docs/PANEL.md) walks through every step.

## Install

You need a NOTE4C, a USB-C data cable and `esptool` on your computer. The
[installation guide](docs/INSTALL.md) has eight steps, and most of the time
goes into two full backups. In short:

1. Back up the whole flash twice.
2. Run [`tools/preflight.py`](tools/preflight.py) on the backups. It compares
   them with the tested NOTE4C and says `READY` or `STOP`.
3. Write two files from the [0.4.2 release](https://github.com/fiedoruk/emini-home/releases/tag/v0.4.2):
   the partition table at `0x8000` and the application at `0x20000`. The
   bootloader and factory data stay untouched.
4. Verify, start and continue on your phone.

## Status

Where 0.4.2 stands:

- **Tested on one NOTE4C Devkit** (ESP32-S3, 16 MiB flash, factory partition
  layout). This release was written and verified on that unit with the
  sequence from the installation guide: backups, preflight check, write and
  verify.
- **Phone setup** has been done end to end from an iPhone on that unit, from
  the setup Wi-Fi to the panel on the home network. Android phones have not
  been tried yet.
- **Starting over**, which erases only the settings area, has been done on
  that unit: the area read back empty and Home opened the setup screen.
- **A full picture change takes about 25 seconds**, measured on that unit
  over USB power.
- **Location** comes from a town you search for in the panel, or from an
  estimate based on your internet address, which can land on your provider's
  city.
- **Battery life has not been measured.** The panel shows voltage and a rough
  percentage. Wi-Fi stays on and there is no deep sleep yet.
- **Going back to the factory firmware** uses the standard esptool procedure
  and has not yet been tried on a real unit.
- **Updates** are installed over USB. There is no over-the-air update mechanism.

If you install it, a [hardware report](https://github.com/fiedoruk/emini-home/issues/new?template=hardware-report.yml)
helps the next person decide.

## Privacy

Home talks to MET Norway for weather, to the news feed you choose, to FreeIPAPI
for an approximate location and to public time servers. When you search for a
town, your phone's browser sends the search to Open-Meteo. Each of these
services sees an ordinary request from your internet address. Nothing goes to
emini, and the firmware has no analytics. The panel runs over HTTP on your
local network and settings are stored on the device without encryption, so
keep the device on a network and in a home you trust. Details:
[privacy](docs/PRIVACY.md).

## Build it yourself

The firmware builds with ESP-IDF v6.0 and nothing else; see
[building from source](docs/BUILD.md). Hardware details and the flash layout
are in [hardware](docs/HARDWARE.md).

## How it was built

One person directing two AI coding agents, Claude Code and Codex. I set the
direction, made the calls and tested the firmware on a NOTE4C.

## Security

Please report vulnerabilities privately through
[GitHub security advisories](https://github.com/fiedoruk/emini-home/security/advisories/new).
[SECURITY.md](SECURITY.md) lists the known limits of this release.

## Credits and licence

emini Home is released under the [MIT License](LICENSE).

- Weather data from [MET Norway](https://api.met.no/), licensed CC BY 4.0.
- Place search by [Open-Meteo.com](https://open-meteo.com/), using location
  data from [GeoNames](https://www.geonames.org/), licensed CC BY 4.0.
- Approximate location from [FreeIPAPI](https://freeipapi.com/).
- Headlines belong to their publishers; the default feed is BBC World.
- Display driver and battery curve adapted from the
  [NOTE4C reference firmware by LazyYoun](https://github.com/LazyYoun/youn-ink-fourcolor-firmware) (MIT).
- Typeface: [Atkinson Hyperlegible Next](https://github.com/googlefonts/atkinson-hyperlegible-next) (SIL OFL 1.1).
- Chinese glyphs: [Noto Sans CJK SC](https://github.com/notofonts/noto-cjk) (SIL OFL 1.1), GB 2312 level 1.
- Small text (12 and 16 px): [TRMNL12 and TRMNL16](https://github.com/usetrmnl/trmnl-framework) pixel fonts by Heavyweight Digital Type Foundry for TRMNL (SIL OFL 1.1).
- Built on [ESP-IDF](https://github.com/espressif/esp-idf); all components and
  licences are listed in [third-party notices](THIRD_PARTY_NOTICES.md).

ZECTRIX and NOTE4C may be trademarks of their owner. The author bought the
NOTE4C used for development at the retail price. emini Home is an independent
project, not made or sponsored by ZECTRIX.
