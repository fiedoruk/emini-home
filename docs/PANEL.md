# Set up emini Home from your phone

emini Home has no app. The NOTE4C serves its own settings page, and your phone
opens it in the browser. Your settings are saved on the device; the browser
keeps only its access token and the panel's own language and theme.

<p align="center">
  <img src="images/panel-pair.webp" width="220" alt="Pairing page">
  <img src="images/panel-home.webp" width="220" alt="Home tab with the picture on the display">
  <img src="images/panel-rhythm.webp" width="220" alt="Rhythm tab">
</p>

## 1. Connect your phone

On first start, the display shows **Connect your phone** with two QR codes.
Once the current picture has finished drawing, you can also open the setup
window by holding **OK / BOOT for 2 seconds**. It stays open for 5 minutes.

1. Scan the first QR code, or join the Wi-Fi network **emini.ink** with the
   password printed on the display.
2. Scan the second QR code, or open `http://192.168.4.1`. Joining the network
   does not open the page by itself.
3. On the **Make yourself at Home** page, type the six-digit code from the
   display and tap **Connect this phone**. The browser remembers the connection.
   The device keeps up to four browser connections, and each address counts
   separately: a phone paired at `192.168.4.1` and again at the device's
   home-network address uses two. Tap **Disconnect this phone**, under
   **Settings → Your device**, to free the connection for the address you are
   on. If all four connections are taken and you no longer have one of those
   browsers, see [starting over](INSTALL.md#starting-over).

**Where the panel lives afterwards.** The **emini.ink** network and
`http://192.168.4.1` exist only while the setup window is open, for 5 minutes.
Once Home is on your home Wi-Fi, the panel is always there at the device's own
address, shown in **Settings → Your device** and on the **Continue on your home
network** card: for example `http://192.168.1.23`, or `http://home-1a2b.local`
on phones that resolve `.local` names. Bookmark it; you do not need the button
for everyday use.

<p align="center"><img src="images/epaper-setup.png" width="400" alt="Setup screen on the display"></p>

## 2. Bring the device onto your Wi-Fi

Right after pairing, the panel shows **Your home Wi-Fi** with a list of nearby
networks. Pick yours and enter its password. The NOTE4C joins 2.4 GHz networks
with WPA2 or WPA3 Personal; open networks are not supported, and a hidden
network can be typed in by name. You can also choose **Set up later** and find
the same page under **Settings → Wi-Fi connection**.

<p align="center"><img src="images/panel-wifi.webp" width="260" alt="Your home Wi-Fi: nearby networks with signal strength"></p>

When the device joins your network, your phone may lose the setup network, and
the setup network closes anyway when the 5-minute window ends. Reconnect the
phone to your home Wi-Fi and open the local address shown in the panel. Your
browser treats the new address as a new site, so the panel asks for a pairing
code again. If the setup window has closed by then, hold OK / BOOT for 2 seconds
to open a new one.

## 3. The Home tab

The **On your Home** section shows the last picture the display confirmed, pixel
for pixel, with its status and the battery. Below it, **Your screens** lists the
five kinds of information:

| Screen | What it shows |
| --- | --- |
| Weather | The forecast from MET Norway for the place you set |
| News | One headline from a public RSS or Atom feed: the first item for BBC World (the default), the newest entry for other HTTPS feeds |
| Your note | A few words of your own |
| Sky | Sunrise, sunset and the moon, worked out on the device from the place you set; nothing is downloaded |
| Air | Air quality, UV and pollen from Open-Meteo for the place you set |

Sky and Air are new, and they start switched off: your display keeps showing
what it showed before. Air's picture arrives with the next update, so until then
that screen says so when you switch it on. Turn one on, or off, under **In your
collection** on its page.

Tap a screen to open its page:

- **Preview of saved settings**: a 1:1 view of the picture. It updates when new
  information arrives. Until Home has downloaded the forecast or the feed,
  every composition shows the same placeholder.
- **Source & updates** (Weather and News): when the information was issued,
  downloaded and last checked, and the time after which Home checks again.
  **Check for updates** asks for a new download, which Home makes only once
  the source's cache time has run out.
- **What it says**: for Weather, the place (see below). For News, the feed
  address. For your note, the words.
- **How it looks**: the **Composition** and a link to the texture, colour and
  text size.
- **In your collection**: include the screen in the rotation and change its
  order.

<p align="center"><img src="images/panel-edit.webp" width="260" alt="The Weather screen page with a preview of the saved settings, source and update times, and the saved location"></p>

Tap **Save settings** to store your changes on the device, then **Show now** to
put the saved picture on the display. The pigments need about 25 seconds to
settle, and button presses during that time are ignored.

### The place for the weather

**Search for your town** on the Weather page: type a name, and the panel lists
matching places with their region and country. Pick one, and Home saves its
name, coordinates and time zone. The search goes from your phone straight to
Open-Meteo, so it works only while your phone has internet access, for example
on your home Wi-Fi, and not on the setup network. Place search by
[Open-Meteo.com](https://open-meteo.com/), using location data from
[GeoNames](https://www.geonames.org/), licensed under CC BY 4.0.

<p align="center"><img src="images/panel-location-search.webp" width="260" alt="Town search on the Weather page: Warsaw typed in, matching places with their region and country, and the Open-Meteo and GeoNames credit"></p>

**Use my location** estimates an area from the device's internet address
instead. The estimate can land on your internet provider's city, and it does
not replace a town you picked in the search.

### The Sky screen

Sky asks nothing of the internet: the device works out sunrise, sunset, the
length of the day and the phase of the moon from the place you set and from its
own clock. Nothing is downloaded for this screen, so it keeps working when the
network does not.

<p align="center"><img src="images/panel-sky.webp" width="260" alt="The Sky screen page in the panel: a preview and the note that everything is computed on the device"></p>

### The Air screen

Air shows the European air quality index, PM2.5 for the next 24 hours, the UV
index with a sunscreen hint and, in Europe, four pollens, from Open-Meteo's
Air Quality service. On its page you choose the headline number: the European
index, the US AQI or PM2.5. Home asks Open-Meteo only while this screen is
switched on, about once an hour, and sends it the saved coordinates and
nothing else.

<p align="center"><img src="images/panel-air.webp" width="260" alt="The Air screen page in the panel: the Open-Meteo source line and the choice of the headline number"></p>

### Compositions

Every screen can be drawn as **Print**, **Rhythm** or **Atlas**. Choose
**In turn** to use all three: Home shows Print, then Rhythm, then Atlas, and
moves to the next composition each time that screen comes back to the display.
When the same screen stays on the display, the composition changes after each
interval, but not during quiet hours. Set it right under the composition
choices: **Change composition every … minutes** (5 to 1440), separate from the
Rhythm tab.

<p align="center"><img src="images/panel-composition-cycle.webp" width="260" alt="How it looks on the Weather page: Composition set to In turn, a note that Print, Rhythm and Atlas take turns, and Change composition every … minutes set to 30"></p>

## 4. The Rhythm tab

**How screens change**:

- **One screen** keeps the same screen and redraws it when its information
  changes.
- **Day rhythm** switches between screens at three times of day you choose, on
  the days you choose.
- **Rotation** moves to the next screen every 5 to 1,440 minutes.

Automatic changes skip screens that have no information yet, such as Weather
before you set its place.

**Quiet hours** keep the last picture on the display overnight. This is not a
sleep or battery setting. The **Take a pause** setting controls how long
automatic changes wait after you press a physical button or tap **Show now**.
Tap **Pause for 60 min** to hold them for an hour.

## 5. The Settings tab

- **Wi-Fi connection**: change the network.
- **Battery**: voltage, charging state and a rough percentage.
- **Appearance**: **Pixel texture** (Fine, Medium or Large), **Colour use**
  (Black & white, Balanced, Expressive), **Brush** and **Larger text**, with a
  preview. Fine keeps 1-pixel patterns for black and paper; coloured patterns
  are never finer than 2 pixels. The brush is the pattern that paints every
  tone on the large fields: **Grain** (blue noise, the default), **Halftone
  dots**, or the classic **Grid** of earlier versions. Profiles (**Desk**, **At a glance**, **Showcase**) give
  you a starting point that you can adjust.
- **Preferences**: display language, time zone and units.
- **Your device**: connection details and help. **Disconnect this phone**
  removes this browser's access.

<p align="center"><img src="images/panel-appearance.webp" width="260" alt="Appearance settings: Pixel texture Fine, Medium or Large, and Colour use Black and white, Balanced or Expressive, with a preview below"></p>

The **PL / EN** button at the top of the panel changes the language of the
panel itself, and the moon or sun button switches it between light and dark.

## 6. The Share tab

**Download your recipe** saves your compositions, appearance, screen order,
language, units, clock format and rhythm, including quiet hours, as a small
file. It leaves out your location, time zone, note, feed address and
connection details. Someone else can import it with **Bring a recipe home**.
You can also download a public sample picture, or choose to download the
picture on your own display. Check that one for personal words or your place
name before you share it; the setup screen cannot be downloaded.

## Good to know

- On the device, **Up** and **Down** switch between screens. A short press on
  **OK / BOOT** switches the display language between English and Polish, or
  does the job you choose in **Settings → Preferences**: check for updates,
  hold the current screen (press again to resume) or open the setup window.
  Holding OK / BOOT for 2 seconds always opens the setup window.
  On a screen set to **In turn**, Down and Up first step through its three
  compositions and then move to the next screen.
- The panel talks to the device over HTTP on your local network. Use it on a
  home network you trust. See [privacy](PRIVACY.md).
- To clear all settings and pairings, see
  [starting over](INSTALL.md#starting-over).
