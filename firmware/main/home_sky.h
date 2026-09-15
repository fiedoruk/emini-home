#ifndef HOME_SKY_H
#define HOME_SKY_H
#include <stdbool.h>
#include <stdint.h>

/* Sun and Moon for one local calendar day, for the "Sky" screen.
 * Pure, reentrant C11: no heap, I/O, global mutable state or device dependency.
 * The module knows nothing about time zones. The caller resolves local midnight
 * (home_tz_localtime in home_places.h) and hands over its UTC instant; every
 * value returned here is a UTC epoch second, so a DST change costs nothing. */

/* Moon phase buckets, 45 degrees of elongation each. Names live in the
 * renderer, because they are translated. */
enum {
    HOME_MOON_NEW = 0,
    HOME_MOON_WAXING_CRESCENT = 1,
    HOME_MOON_FIRST_QUARTER = 2,
    HOME_MOON_WAXING_GIBBOUS = 3,
    HOME_MOON_FULL = 4,
    HOME_MOON_WANING_GIBBOUS = 5,
    HOME_MOON_LAST_QUARTER = 6,
    HOME_MOON_WANING_CRESCENT = 7
};

typedef struct {
    /* UTC epoch seconds; 0 when the event does not happen on this day.
     * Sunrise/sunset use the standard zenith 90.833 deg (refraction plus solar
     * semidiameter), civil dawn/dusk use 96 deg. solar_noon is always set. */
    int64_t sunrise, sunset;
    int64_t civil_dawn, civil_dusk;
    int64_t solar_noon;
    /* Set when the disc centre stays above (-0.833 deg) or below it all day. */
    bool polar_day, polar_night;
    /* Day length and its change against the previous local day. 86400 on a
     * polar day, 0 on a polar night. */
    int32_t day_length_s, day_length_delta_s;
    /* Moon as seen at local noon. Age is measured from the preceding new moon,
     * fraction is the illuminated part of the disc, 0..1. */
    double moon_age_days, moon_fraction;
    int moon_phase;                /* one of the HOME_MOON_* buckets */
    int days_to_full, days_to_new; /* whole days, rounded; 0 means today */
} home_sky_t;

/* Fills out for the local day that starts at local_midnight_utc. False (and an
 * untouched out) for a NULL out, a non-finite or out-of-range coordinate.
 * Latitude -90..90, longitude -180..180, east positive. */
bool home_sky_day(double lat, double lon, int64_t local_midnight_utc, home_sky_t *out);

/* Geometric altitude of the Sun's centre in degrees, refraction not applied.
 * Feeds the 24 h altitude curve. Returns 0 for a bad coordinate. */
double home_sky_altitude(double lat, double lon, int64_t utc);

#endif
