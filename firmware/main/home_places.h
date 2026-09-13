#ifndef HOME_PLACES_H
#define HOME_PLACES_H
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "cJSON.h"

/* Time-zone catalogue only. Location is selected automatically by the phone. */
cJSON *home_timezones_json(void);

/* Existence/canonical-name lookup ONLY. Never pass this result to setenv(TZ).
 * Returns a stable IANA name, or NULL for an unknown name. */
const char *home_timezone_lookup(const char *name);

/* Pinned tzdb 2026c, 1970-01-01 <= UTC < 2041-01-01.
 * False for unknown name/out-of-range; outputs are not changed on failure.
 * No process-global TZ mutation, so forecast/publication timestamps are safe.
 * localtime returns calendar fields and tm_isdst; use offset_at for numeric
 * offsets. Do not format %z/%Z from implementation-specific struct tm fields. */
bool home_tz_offset_at(const char *name, int64_t utc, int32_t *offset_seconds,
                       bool *is_dst);
bool home_tz_localtime(const char *name, int64_t utc, struct tm *out);

#endif
