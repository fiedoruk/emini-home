# Install emini Home

> A web installer at [esp32ai.me/install](https://esp32ai.me/install?fw=emini-home) can write the same
> files from Chrome or Edge, with the file or a link to it. This guide is the careful
> route: two full backups and a preflight check before anything is written.

This guide installs emini Home 0.6.2 on a **ZECTRIX NOTE4C Devkit** from a
computer, using Espressif's `esptool`. Most of the time goes into two full
backups of the 16 MiB flash.

The esptool commands in steps 3, 4, 6, 7 and 8 and under *Starting over* are
the ones we ran on our test unit. That unit is the only one this release has
been tested on, so the guide starts with a backup and a check that tells you to
stop if your NOTE4C's boot area or partition layout looks different. Going back
to the factory firmware has not yet been tried on a real NOTE4C.

## What you need

- A ZECTRIX NOTE4C Devkit with the four-colour 400 × 300 display. The
  monochrome NOTE4 is a different device and is not supported. The preflight
  check cannot tell the two apart, so check which one you have before you
  start.
- A USB-C cable that carries data, not only power.
- A computer with Python 3.10 or newer and esptool 5.4.0, the version this
  guide was tested with. Install it in its own environment:

  ```sh
  python3 -m venv esptool-env
  . esptool-env/bin/activate
  python -m pip install esptool==5.4.0
  esptool version
  ```

  On Windows, use `py -m venv esptool-env` and `esptool-env\Scripts\activate`
  for the first two lines. The last command must print `5.4.0`. If you open a
  new terminal later, run the activate line again before the next esptool
  command.

- From the [v0.6.2 release](https://github.com/fiedoruk/emini-home/releases/tag/v0.6.2):
  `emini-home-0.6.2-note4c.bin`, `emini-home-0.6.2-partition-table.bin`,
  `emini-home-0.6.2-sdkconfig.txt` and `SHA256SUMS`.
- A copy of this repository at tag `v0.6.2`, for `tools/preflight.py`.
  Download **Source code (zip)** from the same release and unpack it outside
  your installation folder. It unpacks into a folder named `emini-home-0.6.2`.

## What changes on the device

Only two areas of the 16 MiB flash are written.

| Address | Size | Before | After |
| --- | --- | --- | --- |
| `0x8000` | 3 KiB | factory partition table | same table plus one 64 KiB settings area, `home_nvs` at `0x10000` |
| `0x20000` | about 3.6 MiB of the 4,032 KiB application slot | factory application | emini Home |

The bootloader (`0x0`), factory NVS (`0x9000`), boot selection data
(`0xD000`), PHY data (`0xF000`), the second application slot (`0x410000`)
and the assets area (`0x800000`) are not written. No installation step writes
or erases the whole chip, and eFuses and security keys stay untouched. Your
full backup from step 4 is what lets you go back.

## 1. Check the files you downloaded

Make a new, empty folder for this installation, put the four release files in
it and run, from that folder:

```sh
shasum -a 256 -c SHA256SUMS
```

On Linux use `sha256sum -c SHA256SUMS`. All three lines must end in `OK`. If a
line says `FAILED` or a file is missing, download that file again and do not
continue. On Windows, run
`certutil -hashfile emini-home-0.6.2-note4c.bin SHA256` and
`certutil -hashfile emini-home-0.6.2-partition-table.bin SHA256`, and compare
each result with its line in `SHA256SUMS`.

## 2. Connect the NOTE4C and find its port

Connect only the NOTE4C and unplug any other ESP32 board. Close anything else
that talks to serial ports (Arduino IDE, a serial monitor, another flashing
tool). Since 0.6.0 Home sleeps between events on battery, and its USB port
sleeps with it, so the computer may not see the device at all. If emini Home is
already installed, first hold the round OK button for 2 seconds until the setup
screen appears: while that window is open Home stays awake. Then plug the cable
in, or unplug it and plug it in again. The NOTE4C shows up as a USB JTAG/serial
device:

- macOS: `/dev/cu.usbmodem…`
- Linux: `/dev/ttyACM0` or similar. If esptool reports *Permission denied*,
  add your user to the `dialout` group and log in again.
- Windows: `COM3` or similar, in Device Manager

In the commands below, replace `PORT` with that name. Every esptool command
prints the chip's `MAC` address. It must be the same in every step; if it
changes, stop.

## 3. Confirm the chip is not locked

If emini Home is already installed, first wait until the display has stopped
changing.

```sh
esptool --chip esp32s3 -p PORT --after no-reset --no-stub get-security-info
```

The output must include `Secure Boot: Disabled` and
`Flash Encryption: Disabled`. If either one says enabled, stop here: this
release has only been tested on an unlocked NOTE4C.

`--after no-reset` keeps the NOTE4C in its download mode until step 8, so
nothing on the device changes between the checks and the installation. If you
stop before step 8, run the command from step 8 to start your firmware again.

## 4. Back up the whole flash, twice

Run these commands in the folder from step 1. esptool replaces an existing
file with the same name without asking, so never run them in a folder that
already holds a backup. The backup from your first installation is your only
copy of this unit's factory firmware and data: keep it, with a second copy on
another disk, for as long as you have the device.

```sh
esptool --chip esp32s3 -p PORT -b 460800 --after no-reset read-flash 0 ALL note4c-backup-a.bin
esptool --chip esp32s3 -p PORT -b 460800 --after no-reset read-flash 0 ALL note4c-backup-b.bin
```

Each file is 16 MiB. The backup can contain Wi-Fi passwords and other data
from the firmware on the device, so keep both copies somewhere private and
never attach them to an issue.

## 5. Run the preflight check

Run this from the installation folder, with the path to the unpacked source
code (on Windows, type `py` instead of `python3`):

```sh
python3 path/to/emini-home-0.6.2/tools/preflight.py note4c-backup-a.bin note4c-backup-b.bin emini-home-0.6.2-partition-table.bin emini-home-0.6.2-note4c.bin
```

The check reads the two backups, confirms they are identical and compares the
bootloader, boot selection data, partition table and the future settings area
with the NOTE4C this release was tested on. It also checks that the partition
table file is the emini Home table and that the application file is an emini
Home image for the ESP32-S3, and prints the version stored in that image: the
`Application file` line must say `0.6.2`. After `READY` it names the two files
for step 6. It never connects to the device, and it cannot tell a monochrome
NOTE4 from a NOTE4C, so continue only if your device has the four-colour
display. Save the output next to your backups; its `Backup SHA-256` line
identifies them.

- `READY: first installation.` Continue with step 6. Your two backups hold the
  factory firmware, so keep them.
- `READY: update or reinstall.` emini Home is already installed, and step 6
  keeps your Home settings. Your two new backups hold emini Home; the factory
  backup is the one from your first installation.
- A line that starts with `ERROR:` means the check could not read a file.
  Check that the four release files are in this folder and that the path to
  `preflight.py` is right, then run the check again.
- Anything else, including a line that starts with `STOP:`, means do not
  write. If the line asks you to read the flash again, make a new, empty
  folder, put the four release files in it as in step 1 and repeat steps 4 and
  5 there: esptool overwrites a file with the same name, and the backup from
  your first installation is the only copy of the factory firmware. If you have not written anything yet, your NOTE4C is not broken; it
  just differs from the tested unit. Do not erase anything to make the check
  pass. Run the command from step 8 to start your firmware again, and you can
  [open an issue](https://github.com/fiedoruk/emini-home/issues/new/choose)
  with the message (not the backup).

If a write in step 6 was interrupted, follow *Writing stopped halfway* under
[If something goes wrong](#if-something-goes-wrong) instead of this step.

## 6. Write emini Home

Continue only if step 5 printed `READY` for the backups you have just made.
First confirm that the NOTE4C still holds exactly that backup:

```sh
esptool --chip esp32s3 -p PORT --after no-reset verify-flash 0x0 note4c-backup-a.bin
```

It must report `Verification successful (digest matched).` If it does not,
make a new, empty folder, put the four release files in it as in step 1, and
start again from step 4 in that folder. Then write the two files:

```sh
esptool --chip esp32s3 -p PORT -b 460800 --after no-reset write-flash --flash-mode keep --flash-size keep --flash-freq keep 0x8000 emini-home-0.6.2-partition-table.bin 0x20000 emini-home-0.6.2-note4c.bin
```

`keep` stops esptool from changing the flash settings stored in the bootloader.
It only has an effect on a file written at `0x0`, as when you go back to the
factory firmware. Keep the cable connected until the command finishes.

Every emini Home release so far has used the same partition table, so on a
device that already runs Home the table write changes nothing. That is why the
release notes describe an update as the application at `0x20000` alone; this
guide writes both files so that one sequence serves a first installation and an
update.

## 7. Verify what was written

```sh
esptool --chip esp32s3 -p PORT --after no-reset verify-flash 0x8000 emini-home-0.6.2-partition-table.bin 0x20000 emini-home-0.6.2-note4c.bin
```

Both regions must report that the digest matched. If one does not, do not
start the device yet. Follow *Writing stopped halfway* under
[If something goes wrong](#if-something-goes-wrong).

## 8. Start emini Home

```sh
esptool --chip esp32s3 -p PORT run
```

The display changes after about 25 seconds. After a first installation it
shows the setup screen with two QR codes; continue with
[setting up from your phone](PANEL.md). After an update it shows your screens
again. An update from 0.5.x or earlier keeps your settings and starts in **Breath**,
the power mode that turns Wi-Fi off between downloads: press the round OK button
once before you open the panel, and it answers for five minutes (see
[Breath and Open](PANEL.md#breath-and-open)). Going back to an earlier release
afterwards starts that release with default settings, because it does not know
the new power setting.

## Going back to the factory firmware

Use the backup from before your first installation, and run the commands from
the folder that holds it. Run the preflight check from step 5 on it first: it
must print `READY: first installation.` and the `Backup SHA-256` line you
saved then. Otherwise these files are not your factory backup, so do not
write them.

```sh
esptool --chip esp32s3 -p PORT -b 460800 --after no-reset write-flash --flash-mode keep --flash-size keep --flash-freq keep 0x0 note4c-backup-a.bin
esptool --chip esp32s3 -p PORT --after no-reset verify-flash 0x0 note4c-backup-a.bin
esptool --chip esp32s3 -p PORT run
```

If the second command does not report
`Verification successful (digest matched).`, run the first command again
before you start the device.

This puts back the exact flash contents of that backup, including the factory
application and its settings. It is the standard esptool procedure, but we
have **not yet run this restore on a real NOTE4C**. Only use a backup made
from the same unit.

## Starting over

This clears Home's settings, Wi-Fi details and paired browsers by erasing only
the 64 KiB settings area. We ran these commands on our test unit: the settings
area read back empty, and Home opened the setup screen.

Use it only on a NOTE4C that runs emini Home. First make new backups and run
the check (steps 1 to 5, in a new folder with the four release files), and
continue only if it printed `READY: update or reinstall.` On any other device
this area can hold factory data. Copy the command instead of typing it: a
wrong address can erase the bootloader or the factory data.

```sh
esptool --chip esp32s3 -p PORT --after no-reset erase-region 0x10000 0x10000
esptool --chip esp32s3 -p PORT run
```

Home then starts as it does after a first installation and opens the setup
screen. Do this before you give the device to someone else, after you lose a
paired phone, or if Home cannot start because its settings area cannot be
read: it then restarts over and over while the display keeps its last
picture. It does not clear the factory firmware's own settings area at
`0x9000`, which can still hold Wi-Fi details saved by the factory firmware; see
[privacy](PRIVACY.md#on-the-device).

## If something goes wrong

- **The port is busy or esptool cannot connect.** Close other serial programs
  and run the command again. If the port is missing, Home is probably asleep:
  hold the round OK button for 2 seconds until the setup screen appears, then
  unplug the cable and plug it back in. If esptool still reports
  `No serial data received` or cannot connect, as a last resort start the NOTE4C
  in download mode by hand: hold the round OK button, press the reset pinhole
  once, then let go of OK. The display does not change. Add `--before no-reset`
  after `--chip esp32s3` to each esptool command you run while the NOTE4C is in
  download mode.
- **The web installer stopped with an error.** The NOTE4C can stay in its
  download mode, and on battery unplugging the cable does not end it: the display
  keeps its last picture while the battery runs down. Press the reset pinhole
  once to start Home again, then try the installation again.
- **Writing stopped halfway.** Do not erase the chip, and do not make new
  backups over your old ones. Reconnect, run the second command of step 6 and
  then step 7 again, or restore your backup. If esptool still cannot connect,
  open an issue before you try anything else.
- **The display does not change after step 8.** Wait a full minute. Run step 7
  again to check the write, then step 8 to start the device, because step 7
  leaves it in download mode. Wait another minute. If step 7 passes and the
  display still does not change, open an issue before you restore your backup.
- **Home shows the setup screen or default settings after an update.** Home
  found saved settings it could not use. It starts with default settings, or
  with the setup screen when the Wi-Fi and pairing details are affected,
  instead of stopping, and it erases nothing. Set it up again in the panel,
  and please open an issue with the version you updated from. If the settings
  area as a whole cannot be read, Home cannot start and restarts over and over;
  *Starting over* clears that area.

Please do not use `erase-flash`, `idf.py flash`, merged images from other
projects or eFuse commands on a NOTE4C you want to keep. They write the
bootloader, remove the factory data your backup protects, or change the chip
permanently.
