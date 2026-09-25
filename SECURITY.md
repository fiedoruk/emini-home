# Security

## Reporting a vulnerability

Please report security problems privately through GitHub:
**Security → [Report a vulnerability](https://github.com/fiedoruk/emini-home/security/advisories/new)**.
Do not open a public issue for them.

A good report names the firmware version, what you did and what happened.
Please leave out Wi-Fi passwords, pairing codes, access tokens and flash
backups; a redacted description is enough to start. This is a small community
project, so replies are best effort.

## Supported versions

| Version | Security fixes |
| --- | --- |
| 0.6.x (final version) | best effort |

0.6 is the final version and no further feature releases are planned. 0.6.1 and
0.6.2 are maintenance releases: 0.6.1 keeps the status LED off, and 0.6.2 fixes what a
review of 0.6.1 found. If another fix were ever needed, it would come as a new release, installed over USB with the
[installation guide](docs/INSTALL.md). The firmware has no over-the-air update
mechanism and does not check for new versions. Keep the backup from your first
installation: it is your only copy of this unit's factory firmware and data.

## Security model

emini Home is built for a home network you trust and for a device you keep in
your own home. This release has not had an independent security audit. The
following limits are known in this release:

- **The phone panel uses HTTP on the local network.** Someone who can watch or
  change traffic on your Wi-Fi can see what the panel sends, including the
  Wi-Fi password when you set it and the access token your browser sends with
  every request. A copied token keeps working until that browser is
  disconnected in the panel or the settings area is erased.
- **Flash contents are not encrypted.** Secure Boot, flash encryption and NVS
  encryption are off, and the USB serial/JTAG interface stays enabled.
  Physical access with a USB cable gives full access to the device: anyone can
  read the stored home Wi-Fi password, install different firmware, or use the
  serial service commands to open the setup window and get a pairing code.
- **The setup network password stays the same until the settings area is
  erased.** It is generated once on each device, shown on the display and used
  whenever the 5-minute setup window is open. The setup screen can stay on the
  display after the window closes. Anyone who knows the password can join the
  setup network while the window is open and, from within range, decrypt what
  a phone sends over it, including your home Wi-Fi password during setup. See
  [starting over](docs/INSTALL.md#starting-over).
- **Pairing attempts are limited per setup window.** A device on the same
  network can use up those attempts; opening a new window resets them.
- **The panel can only disconnect the browser you are using.** To remove every
  paired browser, for example after losing a phone, erase the settings area
  over USB; this also clears your Wi-Fi details and settings. See
  [starting over](docs/INSTALL.md#starting-over).
- **No button on the device resets the settings.** Erasing only the settings
  area over USB clears them, and this has been done on the test unit; see
  [starting over](docs/INSTALL.md#starting-over). Saved settings that Home
  cannot use are left in place: Home starts with default settings, or with the
  setup screen when the Wi-Fi and pairing details are affected. If the settings
  area as a whole cannot be read, Home cannot start and restarts over and over
  until that area is erased.

A paired browser can set any public HTTPS address as the news feed. Home
fetches feeds only over HTTPS on port 443 and only from public internet
addresses, even after a redirect, so a feed address cannot point it at private
addresses on your local network.

Connections from Home to MET Norway, the Open-Meteo Air Quality API, FreeIPAPI
and news feeds use HTTPS (TLS 1.2) with certificate chain and hostname checks
against a root certificate bundle built into the image. The firmware does not check
certificate validity dates, and the bundle changes only with a new release.
The town search does not go through the device: the panel in your phone's
browser sends it to Open-Meteo over HTTPS.
