# Build from source

You do not need to build anything to use emini Home: the release page has
ready images. Build it yourself when you want to change the firmware or check
what the release contains.

## Requirements

- [ESP-IDF v6.0](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32s3/get-started/)
  (tag `v6.0`, commit `662a3be354759d9487bf4b1a629fadb766cb1800`), installed
  with Espressif's instructions for ESP32-S3 and activated in your shell.
- Nothing else. The JSON, QR code and mDNS components are included in
  `firmware/components`, and the font and time-zone tables are pre-generated.

## Build

```sh
cd firmware
export IDF_COMPONENT_MANAGER=0
idf.py -DIDF_TARGET=esp32s3 reconfigure
idf.py build
```

`IDF_COMPONENT_MANAGER=0` keeps the build offline and uses only the components
in this repository, the same way the release was built. In PowerShell, set the
variable with `$env:IDF_COMPONENT_MANAGER=0` instead of `export`.

The results are:

- `build/emini_home_g3.bin`, the application written at `0x20000`
- `build/partition_table/partition-table.bin`, written at `0x8000`

The phone panel in `firmware/ui` is embedded into the application at build
time. The version in the device's status and in its mDNS announcement comes
from `PROJECT_VER` in `firmware/CMakeLists.txt`.

## Generated files

Two files in `firmware/main/generated` are committed as generated C, so the
build needs nothing but ESP-IDF:

- `home_font.c` holds bitmaps of Atkinson Hyperlegible Next 2.001 at 10, 22,
  30, 44, 48 and 64 pixels and of the TRMNL12 Bold and TRMNL16 Bold pixel
  fonts (v1.002) at 12 and 16 pixels, 333 glyphs per size, followed by
  Simplified Chinese glyphs from Noto Sans CJK SC Medium (GB 2312 level 1 and 2,
  6 874 codepoints with punctuation and full-width forms) at 12, 16, 22 and
  30 pixels.
- `home_noise.h` holds the 64 × 64 blue-noise threshold mask of the renderer, written
  by `tools/build_noise.py` (deterministic seed; the header records the SHA-256 of the mask).
- `home_zones.c` holds 598 time zones compiled from the IANA Time Zone
  Database, release 2026c, with transitions up to the start of 2041.

The 12 and 16 px slices are replaced by `tools/build_fonts_pixel.py` from
`TRMNL12-Bold.ttf` and `TRMNL16-Bold.ttf` in the
[trmnl-framework repository](https://github.com/usetrmnl/trmnl-framework)
under `public/fonts/`. The Chinese glyphs are appended by `tools/build_fonts_cjk.py` (Python 3 with
Pillow) from `NotoSansCJKsc-Medium.otf`, available in the
[noto-cjk repository](https://github.com/notofonts/noto-cjk) under
`Sans/OTF/SimplifiedChinese/`; run it on an Atkinson-only `home_font.c` with
the font path as its argument. The scripts that produced the Atkinson and
time-zone tables are not part of this repository. If you need another glyph
or a newer time zone release, please open an issue.

## Comparing with the release

The release configuration is `firmware/sdkconfig.defaults`, expanded by
ESP-IDF v6.0 into the full `sdkconfig` that is attached to the
[v0.5.0 release](https://github.com/fiedoruk/emini-home/releases/tag/v0.5.0)
for reference.

Your application image will not be byte-identical to the release image,
because ESP-IDF stores the build date and time inside it. Everything else
should be. To check, download `emini-home-0.5.0-note4c.bin` from the release
into the `firmware` folder and run, still from `firmware`:

```sh
python3 ../tools/compare_image.py build/emini_home_g3.bin emini-home-0.5.0-note4c.bin
```

`MATCH` means the two images differ only in the build timestamp and the
checksums calculated from it. `MISMATCH` means some other byte differs, and a
line that starts with `ERROR:` means a file could not be read. When we rebuilt
this repository with ESP-IDF v6.0 before publishing, the result was a match,
and the partition table and the full `sdkconfig` were identical to the release
files.

## Flashing your own build

Follow [installation](INSTALL.md) and use your two files instead of the
release files. Skip step 1 for your own application image, because
`SHA256SUMS` only matches the release files. Your
`build/partition_table/partition-table.bin` must have the SHA-256 of the
release table,
`9375ba2d5bf131bdb991c07024cb1cdb40538e14698a602fa7d9c87f57b38946`; if it
differs, you changed the flash layout and the installation guide does not
apply. The preflight check accepts your two files as well, as long as the
table is identical and the project name is unchanged, and it prints the
version from your image. Do not use `idf.py flash`: it also writes a new
bootloader, which is exactly what the installation guide avoids.
