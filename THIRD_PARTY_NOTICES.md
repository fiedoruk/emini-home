# Third-party notices

emini Home's own code is released under the [MIT License](LICENSE). It builds
on the work below. Each component keeps its own licence; where a licence text is
required, it is next to the component source or in [`licenses/`](licenses/).

## Included in this source repository

| Component | Version | Licence | Licence text |
| --- | --- | --- | --- |
| [cJSON](https://github.com/DaveGamble/cJSON), sources unmodified, built with `CJSON_NESTING_LIMIT=16` | 1.7.19 (`c859b25`) | MIT | [`firmware/components/home_json/LICENSE`](firmware/components/home_json/LICENSE) |
| [QR Code generator](https://github.com/nayuki/QR-Code-generator) by Project Nayuki | `3c6d0b3` | MIT | [`firmware/components/home_qr/LICENSE`](firmware/components/home_qr/LICENSE) |
| [Espressif mDNS](https://github.com/espressif/esp-protocols) | 1.12.0 (`db06b19`) | Apache-2.0 | [`firmware/components/mdns/LICENSE`](firmware/components/mdns/LICENSE) |
| NOTE4C display driver and battery curve, adapted from the [reference firmware by LazyYoun](https://github.com/LazyYoun/youn-ink-fourcolor-firmware/tree/51812e4ab3fa80ba7a5a5a274635ca2cf3901a25) | `51812e4` | MIT | notice at the top of [`firmware/main/home_panel.c`](firmware/main/home_panel.c) |
| [Atkinson Hyperlegible Next](https://github.com/googlefonts/atkinson-hyperlegible-next), Copyright 2020-2024 The Atkinson Hyperlegible Next Project Authors; bitmap subset in `firmware/main/generated/home_font.c` | 2.001 | SIL OFL 1.1 | [`licenses/Atkinson-OFL.txt`](licenses/Atkinson-OFL.txt) |
| [Noto Sans CJK SC](https://github.com/notofonts/noto-cjk), Copyright 2014-2021 Adobe (http://www.adobe.com/); bitmap subset (GB 2312 levels 1 and 2, sizes 12/16/22/30 px) appended to `firmware/main/generated/home_font.c` by `tools/build_fonts_cjk.py` | Medium, 2.004 | SIL OFL 1.1 | [`licenses/NotoSansCJK-OFL.txt`](licenses/NotoSansCJK-OFL.txt) |
| [TRMNL12 and TRMNL16 Bold](https://github.com/usetrmnl/trmnl-framework/tree/main/public/fonts), Copyright 2026 Heavyweight Digital Type Foundry s.r.o., commissioned by TRMNL; bitmaps at 12 and 16 px in `firmware/main/generated/home_font.c` by `tools/build_fonts_pixel.py` | 1.002 | SIL OFL 1.1 | [`licenses/TRMNL-OFL.txt`](licenses/TRMNL-OFL.txt) |
| [IANA Time Zone Database](https://www.iana.org/time-zones), compiled table in `firmware/main/generated/home_zones.c` | 2026c | public domain | — |

The battery percentage curve in `firmware/main/home_battery.c` comes from the
same reference firmware as the display driver and is covered by the MIT notice
reproduced in `firmware/main/home_panel.c`.

## Additionally included in the release images

The firmware images on the release page are built with
[ESP-IDF](https://github.com/espressif/esp-idf) v6.0 and the Espressif
toolchain esp-15.2.0_20251204, and link the following.

| Component | Licence | Licence text |
| --- | --- | --- |
| ESP-IDF framework, drivers and libc support code, and the Espressif Wi-Fi and PHY libraries | Apache-2.0 | [`licenses/ESP-IDF-LICENSE.txt`](licenses/ESP-IDF-LICENSE.txt), [`licenses/esp-idf/esp_wifi/lib/LICENSE`](licenses/esp-idf/esp_wifi/lib/LICENSE), [`licenses/esp-idf/esp_phy/lib/LICENSE`](licenses/esp-idf/esp_phy/lib/LICENSE) |
| Parts of the Espressif Wi-Fi library derived from FreeBSD net80211, Copyright (c) 2001 Atsushi Onoe, Copyright (c) 2002-2009 Sam Leffler, Errno Consulting | BSD-2-Clause | [`licenses/net80211-BSD-2-Clause.txt`](licenses/net80211-BSD-2-Clause.txt) |
| FreeRTOS kernel V10.5.1 as modified for ESP-IDF, Copyright (C) 2021 Amazon.com, Inc. or its affiliates; ESP-IDF changes by Espressif | MIT; Apache-2.0 for the ESP-IDF FreeRTOS additions | [`licenses/esp-idf/freertos/FreeRTOS-Kernel/LICENSE.md`](licenses/esp-idf/freertos/FreeRTOS-Kernel/LICENSE.md) |
| Xtensa port, vectors and HAL, Copyright (c) 1998-2021 Tensilica Inc., Copyright (c) 1999-2021 Cadence Design Systems, Inc. | MIT | [`licenses/Xtensa-MIT.txt`](licenses/Xtensa-MIT.txt) |
| TLSF memory allocator, Copyright (c) 2006-2016 Matthew Conte | BSD-3-Clause | [`licenses/TLSF-BSD-3-Clause.txt`](licenses/TLSF-BSD-3-Clause.txt) |
| lwIP TCP/IP stack | BSD-3-Clause | [`licenses/esp-idf/lwip/lwip/COPYING`](licenses/esp-idf/lwip/lwip/COPYING) |
| Mbed TLS 4.0.0 and TF-PSA-Crypto 1.0.0 | Apache-2.0 (dual-licensed Apache-2.0 or GPL-2.0-or-later; used under Apache-2.0) | [`licenses/esp-idf/mbedtls/mbedtls/LICENSE`](licenses/esp-idf/mbedtls/mbedtls/LICENSE), [`licenses/esp-idf/mbedtls/mbedtls/tf-psa-crypto/LICENSE`](licenses/esp-idf/mbedtls/mbedtls/tf-psa-crypto/LICENSE) |
| wpa_supplicant, Copyright (c) 2002-2019 Jouni Malinen and contributors | BSD-3-Clause | [`licenses/esp-idf/wpa_supplicant/README`](licenses/esp-idf/wpa_supplicant/README) |
| http_parser | MIT | [`licenses/esp-idf/http_parser/LICENSE.txt`](licenses/esp-idf/http_parser/LICENSE.txt) |
| picolibc 1.8.10 C library from the Espressif toolchain | BSD-3-Clause, BSD-2-Clause and other permissive licences, listed per file | [`licenses/picolibc/COPYING.picolibc`](licenses/picolibc/COPYING.picolibc) |
| GCC runtime libraries (libgcc, libstdc++) from the Espressif toolchain | GPL-3.0 with the GCC Runtime Library Exception | — |
| Root certificate bundle embedded by ESP-IDF: 144 certificates from the Mozilla CA certificate store (certdata.txt as of 2 December 2025, as extracted by [curl](https://curl.se/docs/caextract.html)); source form in [ESP-IDF v6.0](https://github.com/espressif/esp-idf/blob/v6.0/components/mbedtls/esp_crt_bundle/cacrt_all.pem) | MPL-2.0 | [`licenses/MPL-2.0.txt`](licenses/MPL-2.0.txt) |

The Espressif Wi-Fi and PHY libraries are included as precompiled binaries
from Espressif, without source code.

lwIP files in the images also carry these copyright notices, under the licence
in `licenses/esp-idf/lwip/lwip/COPYING`: Copyright (c) 2001-2004 Swedish
Institute of Computer Science; Copyright (c) 2002-2003 Adam Dunkels;
Copyright (c) 2001-2004 Leon Woestenberg; Copyright (c) 2001-2004 Axon
Digital Design B.V.; Copyright (c) 2002 CITEL Technologies Ltd.;
Copyright (c) 2010 Inico Technologies Ltd.; Copyright (c) 2007-2009 Frédéric
Bernon, Simon Goldschmidt.

wpa_supplicant files in the images also carry these copyright notices, under
the licence in `licenses/esp-idf/wpa_supplicant/README`: Copyright (c)
2019-2020 The Linux Foundation; Copyright (c) 2013 Cozybit, Inc.

## Information services

- **Weather** comes from the [MET Norway](https://api.met.no/) Locationforecast
  API. Data is licensed under CC BY 4.0; the weather screen credits MET Norway.
- **Air quality, UV and pollen** come from the
  [Open-Meteo Air Quality API](https://open-meteo.com/en/docs/air-quality-api),
  only while the Air screen is switched on. Data is licensed under CC BY 4.0; the
  panel credits Open-Meteo on the Air page.
- **News** comes from the RSS or Atom feed you choose. The default is
  [BBC World](https://feeds.bbci.co.uk/news/world/rss.xml). Headlines belong to
  their publishers and are not covered by this project's licence.
- **Place search** in the panel uses the
  [Open-Meteo Geocoding API](https://open-meteo.com/en/docs/geocoding-api),
  called from your phone's browser. Place search by
  [Open-Meteo.com](https://open-meteo.com/), using location data from
  [GeoNames](https://www.geonames.org/), licensed under CC BY 4.0. The panel
  credits both next to the search results.
- **Approximate location** comes from [FreeIPAPI](https://freeipapi.com/),
  under its own terms and limits.

## Trademarks

ZECTRIX and NOTE4C may be trademarks of their owner. The author bought the
NOTE4C used for development at the retail price. emini Home is an independent
project, not made or sponsored by ZECTRIX.
