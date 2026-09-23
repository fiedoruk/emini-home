# Getting help

emini Home is an independent community project, maintained in spare time.
Questions go to [GitHub Issues](https://github.com/fiedoruk/emini-home/issues),
and help is best effort.

Before opening an issue, check the [installation guide](docs/INSTALL.md) and
the [phone panel guide](docs/PANEL.md). If you are reporting a security
problem, use the private channel in [SECURITY.md](SECURITY.md) instead.

A useful issue includes:

- the firmware version and whether this is a first installation, an update or
  a reinstall
- the exact message you saw (for installation, the output of `preflight.py`
  without its `Backup SHA-256` line)
- your phone and browser, if the problem is in the panel
- what you expected and what happened instead

Please never post a flash backup, a Wi-Fi password, a pairing code, an access
token or a photo of the setup screen. They are private to your device.

## Common questions

**The weather is for the wrong town.** Open the Weather page in the panel,
search for your town and pick it from the list. The search needs internet
access on your phone, so use it on your home Wi-Fi. **Use my location**
estimates the area from your internet address instead, and that can point to
your provider's city.

**The picture did not change.** **Preview** and **Save settings** do not update
the display; **Show now** does. A full change takes about 25 seconds. Quiet
hours and a pause hold only automatic changes.

**The compositions look the same in the preview.** Until Home has downloaded
the forecast or the feed, every composition shows the same placeholder. The
preview updates when the information arrives.

**Check for updates on a Weather or News page did nothing.** Check for updates
asks the source again at once. If the provider has nothing newer, the picture
stays as it was. After a failed download Home tries again by itself in about
15 minutes. Firmware updates are a separate thing: you install
them over USB with the [installation guide](docs/INSTALL.md).

**Home forgot its settings after an update.** Home found saved settings it
could not use. It starts with default settings, or with the setup screen when
the Wi-Fi and pairing details are affected, instead of stopping, and it erases
nothing. Set it up again in the panel, and please open an issue with the
version you updated from.

**I cannot open the panel.** In Breath, the default power mode, Wi-Fi sleeps
between downloads: press the round OK button once, wait a few seconds, then reload
the page; the panel answers for five minutes. If it still does not open, wait until the display has finished drawing, then
hold OK / BOOT for 2 seconds to open the setup window for 5 minutes. Join the
**emini.ink** Wi-Fi shown on the display and open `http://192.168.4.1`.
Joining the network does not open the page by itself.

**I want to give the device to someone else.** Erase Home's settings over USB
as described in [starting over](docs/INSTALL.md#starting-over). This clears
your Wi-Fi details, paired browsers and settings. The factory firmware's own
settings area stays, and if the factory firmware was ever connected to Wi-Fi,
it can still hold those details; see [privacy](docs/PRIVACY.md#on-the-device).
