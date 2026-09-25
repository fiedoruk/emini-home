#ifndef HOME_WAKE_H
#define HOME_WAKE_H
#include "home_types.h"
#include <stdbool.h>
#include <stdint.h>

/* When the radio is needed in the Breath power mode, which turns Wi-Fi off between fetches.
 *
 * Pure functions: no locks, no clock of their own, no side effects, so the whole decision can be
 * tested on the host. Monotonic times are microseconds (like esp_timer_get_time), wall-clock times
 * are seconds. */

/* How long the panel stays reachable after a short press of OK or a request from the panel. */
#define HOME_AWAKE_US INT64_C(300000000)

typedef struct {
    bool breath;         /* false: Open mode, Wi-Fi always on */
    unsigned reasons;    /* home_power_snapshot(): cable, charger, setup, maintenance */
    int64_t now;         /* monotonic */
    int64_t awake_until; /* end of the panel window */
    bool busy;           /* a fetch, a request, a scan or a new network is in progress */
    bool fetch_due;      /* home_sources_due() */
    bool clock_unset;    /* SNTP has not set the clock since boot, or not for six hours */
    int64_t hold_until;  /* a few seconds after connecting, for the clock and the .local name */
    int64_t retry_at;    /* after a session that went nowhere, background work waits until then */
} home_radio_in_t;

bool home_radio_wanted(const home_radio_in_t *in);

/* The sources worth asking for, as the refresh bits (1 weather, 2 headline, 4 air): a screen that
 * is switched on and has what it needs, a place or an address. A screen nobody shows sends
 * nothing anywhere and wakes no radio. */
unsigned home_sources_wanted(const home_config_t *c);

/* Whether any source that is switched on falls due within `ahead` seconds of `now` (wall clock).
 * Only next_fetch is read: a refresh request is turned into next_fetch = 0 by the source worker,
 * and a request left behind for a source that cannot run must not keep the radio awake. */
bool home_sources_due(const home_config_t *c, const home_data_t *d, int64_t now, int64_t ahead);

#endif
