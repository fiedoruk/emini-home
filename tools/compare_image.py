#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Tomasz Fiedoruk
"""Check that an application image you built matches a release image.

ESP-IDF writes the build date and time into every image, so two builds of the
same source are never byte-identical. This script ignores exactly the fields
that depend on that timestamp and compares everything else:

  - app description: build time (0x70, 16 bytes), build date (0x80, 16 bytes)
  - app description: SHA-256 of the ELF file (0xB0, 32 bytes)
  - image footer: checksum byte and image SHA-256 (last 33 bytes)

    python3 tools/compare_image.py firmware/build/emini_home_g3.bin emini-home-0.4.1-note4c.bin

Exit code 0 means MATCH and 1 means MISMATCH. Exit code 2 means ERROR: the
comparison did not run, because a file could not be read or the arguments
were wrong. Nothing is written anywhere.
"""

import sys

TIMESTAMP_FIELDS = [(0x70, 16), (0x80, 16), (0xB0, 32)]
FOOTER = 33


def compare(built, release):
    if len(built) != len(release):
        return False, "sizes differ: %d and %d bytes" % (len(built), len(release))
    if len(built) < 0x100 + FOOTER or built[0] != 0xE9 or release[0] != 0xE9:
        return False, "these do not look like ESP32 application images"
    ignored = set()
    for start, length in TIMESTAMP_FIELDS:
        ignored.update(range(start, start + length))
    ignored.update(range(len(built) - FOOTER, len(built)))
    first, count = None, 0
    for i, (a, b) in enumerate(zip(built, release)):
        if a != b and i not in ignored:
            count += 1
            if first is None:
                first = i
    if count:
        return False, "%d bytes differ outside the timestamp fields, first at 0x%X" % (count, first)

    def text(image, offset, length):
        return image[offset:offset + length].split(b"\0")[0].decode("ascii", "replace")

    return True, "same code; built %s %s, release %s %s" % (
        text(built, 0x80, 16), text(built, 0x70, 16),
        text(release, 0x80, 16), text(release, 0x70, 16))


def main(argv):
    if len(argv) == 2 and argv[1] in ("-h", "--help"):
        print(__doc__.strip())
        return 0
    if len(argv) != 3:
        print(__doc__.strip())
        return 2
    images = []
    for path in argv[1:]:
        try:
            with open(path, "rb") as handle:
                images.append(handle.read())
        except OSError as error:
            print("ERROR: cannot read %s (%s)." % (path, error.strerror or "read error"))
            return 2
    same, detail = compare(images[0], images[1])
    print(("MATCH: " if same else "MISMATCH: ") + detail)
    return 0 if same else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
