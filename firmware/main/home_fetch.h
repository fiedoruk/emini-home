#ifndef HOME_FETCH_H
#define HOME_FETCH_H
#include "home_types.h"
#include "esp_err.h"
/* HTTPS over verified, pinned IPv4/TLS1.2. Single caller owns the connection.
 * HTTP headers/body/redirects share an absolute25s deadline; connect gets8s.
 * DNS uses lwIP's bounded resolver and is checked against deadline on return.
 * no_store marks RAM-only responses; persistence must omit that source.
 * Expiration and request floor are independent; manual refresh respects both. */
esp_err_t home_fetch_weather(const home_config_t*,home_weather_t*,int64_t now);
esp_err_t home_fetch_feed(const home_config_t*,home_feed_t*,int64_t now);
/* Explicit area lookup only; fixed FreeIPAPI hostname, same verified transport.
 * Single source-worker caller. Caller zeroes/frees the bounded JSON body. */
esp_err_t home_fetch_location(char **json,size_t *size);
#endif
