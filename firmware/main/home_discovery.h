#ifndef HOME_DISCOVERY_H
#define HOME_DISCOVERY_H

#include "esp_err.h"

/* Call from one startup owner after the default Wi-Fi netifs/event loop exist.
 * This module must be the application's sole mDNS owner. Accepts Home-ABCD or
 * home-abcd, validates exactly four hex suffix characters, and advertises the
 * lowercase name home-abcd.local. It does not change the persisted AP SSID.
 * Repeated start for the same name is idempotent; another name is rejected.
 * ESP_OK means local service registration, not successful client resolution.
 * On error, the caller keeps its numeric IP URL and never requires mDNS. */
esp_err_t home_discovery_start(const char *hostname);

#endif
