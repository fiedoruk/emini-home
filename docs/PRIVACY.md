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
| Your news feed (by default [BBC World](https://feeds.bbci.co.uk/news/world/rss.xml)) | Home | when the last copy of the feed expires, and never sooner than 25 minutes after the previous request | a request for that feed, plus your home IP address |
| [FreeIPAPI](https://freeipapi.com/) | Home | when the panel asks Home for an approximate location | your home IP address, which it uses to estimate a location |
| [Open-Meteo Geocoding API](https://open-meteo.com/en/terms) | your phone's browser, from the panel | only when you search for a town | the text you typed, your phone's IP address and ordinary browser request data |
| `pool.ntp.org` time servers | Home | at start and then hourly | time requests, plus your home IP address |

Requests from Home identify the software with the User-Agent
`emini-home/0.6 (+https://github.com/fiedoruk/emini-home)`: its name and
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

**How often Home asks.** Home asks a provider again once the copy it holds expires, and it
trusts what the provider says — but never sooner than **25 minutes** after the previous
request, whatever the provider declares. That is half an hour, less the few minutes by which
Home may bring a request forward so that two share one trip of the radio. After a failed
request it tries again in about fifteen minutes. That floor matters: weather declares about half an hour and air quality
about an hour, but the default BBC World feed declares a lifetime of about **two seconds**.
Until 0.6.0 that meant a request every few minutes, roughly fifteen an hour, each one carrying
your home IP address to the feed's server. It is now about two an hour. A screen that
repaints every twenty minutes cannot show anything fresher anyway. If that is still
more than you want, point Home at a different feed or switch the news screen off in the panel.

In **Breath**, the default power mode since 0.6.0, the radio is off between these requests.
That changes when they are sent, not what is sent or to whom.

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

Home also keeps a few numbers about **itself**, shown on the emini card and readable by a
paired browser:

- **Counters** since the first start: pictures drawn, hours awake, downloads, and one battery
  reading per day for the last seven days.
- **A power log**: one line per hour for the last week, each with the battery voltage, whether
  it was charging, how many pictures and downloads there were, how many seconds the chip was
  not allowed to sleep, and how many seconds the display spent drawing and the radio was on.
  Taken together this is a record of **when the device was busy and when it sat on a
  charger** — a rough trace of the rhythm of the room it stands in. It never leaves the device
  on its own: `GET /api/power` needs a paired browser, and nothing is sent anywhere. It is stored unencrypted, like everything else here, and
  [erasing Home's settings](INSTALL.md#starting-over) erases it too.

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
