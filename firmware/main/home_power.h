#ifndef HOME_POWER_H
#define HOME_POWER_H
#include "esp_err.h"
#include <stdbool.h>

/* Power management for the "Otwarte" mode: the chip sleeps between events while Wi-Fi stays
 * connected and the panel keeps answering. On a cable, or while the charger says it is working,
 * Home holds power management locks and behaves exactly like 0.5.2 - that removes flashing, the
 * USB maintenance protocol and desk work from the list of things sleep could break. */
esp_err_t home_power_init(void);

/* Called from the main loop. Cheap: reads two flags and moves the locks when the state has held
 * for a few seconds. Returns true while the locks are held (device is tethered). */
bool home_power_tick(void);

/* What can be seen from outside. Without it there is no way to check in the field whether the
 * chip really sleeps: the panel looks the same either way. */
typedef struct {
    bool enabled;        /* power management started at all */
    bool locks_held;     /* we are holding the chip awake right now */
    unsigned reasons;    /* why - a mask of reasons */
    int64_t held_s;      /* total seconds with the locks held */
    int64_t free_s;      /* total seconds the chip was free to sleep */
} home_power_state_t;
void home_power_snapshot(home_power_state_t *out);

#endif
