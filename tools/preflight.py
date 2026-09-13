#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Tomasz Fiedoruk
"""Check a NOTE4C flash backup before installing emini Home.

This script never talks to the device and never uses the network. It reads
two full flash backups made with esptool and, optionally, the two release
files, and tells you whether the installation described in docs/INSTALL.md
is safe to run on that unit.

    python3 tools/preflight.py backup-a.bin backup-b.bin [partition-table.bin app.bin]

Exit code 0 means READY and 2 means STOP. Exit code 1 means ERROR: the check
did not run, for example because a file could not be read. Nothing is written
anywhere.
"""

import hashlib
import os
import sys

FLASH_BYTES = 16 * 1024 * 1024

# Regions and reference hashes of the factory NOTE4C layout that emini Home
# was installed on and verified with. A unit that differs is not rejected as
# broken; it is simply not a layout this release was tested on.
BOOTLOADER = (0x0000, 0x8000, "7d914bc1cd69da88931aa21e7c596b33f70fae0edcad46cdba39b611e305d025")
OTADATA = (0xD000, 0xF000, "8ba3b110139f45443d4f268d1a3373ef99a1718b71d51664531b83ee2d4b91a3")
TABLE_PAGE = (0x8000, 0x9000)
FACTORY_TABLE = "ce40cfe75056ef74bc052942f8a9ee3dce8e5e14ba17a6f63685a8fa0d11a23d"
HOME_TABLE = "21370c84c1a5a10bfab2256c925c3a2d7621c0697b085dc9499fda7d7cccaf25"
HOME_SETTINGS = (0x10000, 0x20000)  # becomes the home_nvs partition
RELEASE_TABLE = "9375ba2d5bf131bdb991c07024cb1cdb40538e14698a602fa7d9c87f57b38946"
APP_SLOT_BYTES = 0x3F0000  # ota_0

# An ESP-IDF application image starts with esptool's 24-byte image header and
# an 8-byte segment header, followed by esp_app_desc_t at offset 0x20.
IMAGE_MAGIC = 0xE9
CHIP_ID = slice(12, 14)  # image header chip_id, little endian
ESP32S3_CHIP_ID = b"\x09\x00"
APP_DESC_MAGIC = slice(32, 36)  # esp_app_desc_t.magic_word
APP_DESC_MAGIC_WORD = b"\x32\x54\xcd\xab"
APP_VERSION = slice(48, 80)  # esp_app_desc_t.version
APP_PROJECT = slice(80, 112)  # esp_app_desc_t.project_name
PROJECT_NAME = b"emini_home_g3"


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def evaluate(first, second=None):
    """Return (ready, lines). `first` and `second` are full backups as bytes."""
    lines = []

    def stop(reason):
        lines.append("STOP: " + reason)
        return False, lines

    if len(first) != FLASH_BYTES:
        return stop("the backup is %d bytes; a complete NOTE4C backup is 16 MiB "
                    "(%d bytes). Read it again with 'read-flash 0 ALL'."
                    % (len(first), FLASH_BYTES))
    lines.append("Backup SHA-256: " + sha256(first))

    if second is not None:
        if len(second) != FLASH_BYTES or sha256(second) != sha256(first):
            return stop("the two backups differ. Reconnect the cable and read both again.")
        lines.append("Second backup is identical.")

    start, end, expected = BOOTLOADER
    if sha256(first[start:end]) != expected:
        return stop("the bootloader is not the one this release was tested with.")
    lines.append("Bootloader matches the tested NOTE4C.")

    start, end, expected = OTADATA
    if sha256(first[start:end]) != expected:
        return stop("the boot selection data is not the one this release was tested with.")
    lines.append("Boot selection data matches the tested NOTE4C.")

    table = sha256(first[TABLE_PAGE[0]:TABLE_PAGE[1]])
    settings_size = HOME_SETTINGS[1] - HOME_SETTINGS[0]
    settings_blank = first[HOME_SETTINGS[0]:HOME_SETTINGS[1]] == b"\xff" * settings_size

    if table == FACTORY_TABLE:
        if not settings_blank:
            return stop("the factory layout is present, but the area Home needs for its "
                        "settings (0x10000-0x1FFFF) is not empty.")
        lines.append("Factory partition layout found; the settings area is empty.")
        lines.append("READY: first installation.")
        return True, lines

    if table == HOME_TABLE:
        lines.append("emini Home partition layout found; your Home settings will be kept.")
        lines.append("READY: update or reinstall.")
        return True, lines

    return stop("the partition table is neither the factory NOTE4C layout nor emini Home's.")


def release_problem(table, app):
    """Return why the files for step 6 must not be written, or None."""
    if sha256(table) != RELEASE_TABLE:
        return "the partition table file is not the emini Home partition table."
    if not (256 < len(app) <= APP_SLOT_BYTES
            and app[0] == IMAGE_MAGIC
            and app[CHIP_ID] == ESP32S3_CHIP_ID
            and app[APP_DESC_MAGIC] == APP_DESC_MAGIC_WORD
            and app[APP_PROJECT].split(b"\0")[0] == PROJECT_NAME):
        return "the application file is not an emini Home image for the ESP32-S3."
    return None


def app_version(app):
    """Return the version stored in the image, reduced to printable characters."""
    text = app[APP_VERSION].split(b"\0")[0].decode("ascii", "replace")
    return "".join(ch if ch.isalnum() or ch in ".-+_" else "?" for ch in text) or "?"


def main(argv):
    if len(argv) == 2 and argv[1] in ("-h", "--help"):
        print(__doc__.strip())
        return 0
    if len(argv) not in (3, 5):
        print(__doc__.strip())
        return 1
    path = argv[1]
    try:
        if os.path.samefile(argv[1], argv[2]):
            print("STOP: give two separate backups, read one after the other.")
            return 2
        files = []
        for path in argv[1:]:
            with open(path, "rb") as handle:
                files.append(handle.read())
    except OSError as error:
        print("ERROR: cannot read %s (%s). Check the file name and run the check again."
              % (error.filename or path, error.strerror or "read error"))
        return 1
    if len(files) == 4:
        problem = release_problem(files[2], files[3])
        if problem:
            print("STOP: " + problem)
            return 2
        print("Application file: emini Home %s for the ESP32-S3." % app_version(files[3]))
    ready, lines = evaluate(files[0], files[1])
    print("\n".join(lines))
    if ready and len(files) == 4:
        print("Step 6 writes: 0x8000 %s 0x20000 %s" % (argv[3], argv[4]))
    return 0 if ready else 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
