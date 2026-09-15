# Privacy

emini Home runs entirely on the NOTE4C. The phone panel, your settings, the
schedule, the rendering and the saved data all live on the device. There is no
emini account, no emini cloud service and no computer that has to stay on. The
firmware contains no analytics or telemetry.

## What leaves the device

Home and the panel talk directly to a few public services. Each of them sees an
ordinary internet request, including an IP address, and its own terms apply.
Nobody behind emini Home receives your searches, your location or your IP
address from any of these requests.

| Service | Sent by | When | What it receives |
| --- | --- | --- | --- |
| [MET Norway](https://api.met.no/) weather API | Home | when the last forecast expires, as MET Norway sets it | the saved forecast location, cut to 4 decimal places, plus your home IP address |
| [Open-Meteo Air Quality API](https://open-meteo.com/en/terms) | Home | only while the Air screen is switched on, about once an hour | the saved location, cut to 4 decimal places, plus your home IP address |
| Your news feed (by default [BBC World](https://feeds.bbci.co.uk/news/world/rss.xml)) | Home | when the last copy of the feed expires | a request for that feed, plus your home IP address |
| [FreeIPAPI](https://freeipapi.com/) | Home | when the panel asks Home for an approximate location | your home IP address, which it uses to estimate a location |
| [Open-Meteo Geocoding API](https://open-meteo.com/en/terms) | your phone's browser, from the panel | only when you search for a town | the text you typed, your phone's IP address and ordinary browser request data |
| `pool.ntp.org` time servers | Home | at start and then hourly | time requests, plus your home IP address |

Requests from Home identify the software with the User-Agent
`emini-home/0.4 (+https://github.com/fiedoruk/emini-home)`: its name and
version, followed by the project page as a contact address, which weather
services ask clients to include. The same text is sent from every device and
does not identify you.

Home also looks up these names through your network's DNS server, gives your
router the name `emini-home` when it joins, and follows up to three HTTPS
redirects from the weather service or your feed, which can lead to other
servers.

## How Home finds its location

There is no GPS. You can set the place for the weather in two ways.

1. **Search for a town.** The panel sends what you type from your phone
   directly to Open-Meteo and lists matching places. Home saves only the place
   you pick: its name, coordinates and time zone. Open-Meteo says it may keep
   IP addresses in its web server logs for technical reasons and deletes those
   logs after 90 days. Place search by [Open-Meteo.com](https://open-meteo.com/),
   using location data from [GeoNames](https://www.geonames.org/), licensed
   under CC BY 4.0.
2. **Approximate location.** Home asks FreeIPAPI where its internet connection
   appears to be. This can point to your internet provider's city rather than
   yours. Home does not replace a town you picked in the search with this
   estimate.

Whichever way you choose, the saved location becomes the forecast location
that Home sends to MET Norway. While the Air screen is switched on, Home sends
the same coordinates to the Open-Meteo Air Quality API about once an hour, for
air quality, UV and pollen. Switch that screen off and Home stops asking.

## On your local network

- The panel is served by the device over **HTTP**, not HTTPS. Use it on a home
  network you trust and never expose the device to the internet.
- A browser gets access by entering a short pairing code shown on the display.
  The device stores only a hash of each browser's access token.
- The device announces itself on the local network as `home-xxxx.local`,
  with its model and firmware version.
- Without pairing, any device on the same network can ask Home for its name,
  local address, firmware version, the screen it shows, whether it is paired
  and whether the setup window is open. Your settings, note, feed address and
  Wi-Fi details need a paired browser.

## On the device

Settings, including the location you saved, the home Wi-Fi password, the setup
network password and cached data are stored in flash **without encryption**.
Anyone with the device and a USB cable can read them. Treat a NOTE4C like a
router you own: keep it in your home, and
[erase Home's settings over USB](INSTALL.md#starting-over) before giving it
away. Installing Home and starting over leave the factory firmware's own
settings area untouched; if the factory firmware was ever connected to Wi-Fi,
that area can still hold its Wi-Fi details.

The flash backup you make during installation contains the same kind of data.
Keep it private.

## Sharing

A recipe exported from the panel contains compositions, appearance, screen
order, language, units, clock format and rhythm choices only, including quiet
hours and day-rhythm times. It does not include your note, location, time
zone, feed address, Wi-Fi details or access token.
Before sharing a photo of the display, check that it does not show the setup
screen, which contains the setup network password.

## The emini.ink website

The website does not talk to your device. It counts page views, visits to
missing pages and clicks on its links, such as the link to GitHub, with a
self-hosted instance of [Plausible Analytics](https://github.com/plausible/analytics)
at `skad.click`, without cookies. The analytics server and the hosting
provider receive ordinary request data, including your IP address, when you
open a page.
