#ifndef HOME_AIR_H
#define HOME_AIR_H
#include "home_types.h"

/* Open-Meteo Air Quality (air-quality-api.open-meteo.com/v1/air-quality, CC BY 4.0)
 * queried with hourly=pm2_5,pm10,european_aqi,us_aqi,uv_index,alder_pollen,
 * birch_pollen,grass_pollen,mugwort_pollen, forecast_days=2 and timezone=UTC.
 * Same contract as the other parsers: at most 128 KiB in, never fetches anything,
 * assigns *out only on success, and the caller owns the HTTP freshness fields.
 * home_air_t itself lives in home_types.h, next to the other cached sources.
 * Index 0 is the last full hour at or before now and is never more than one hour
 * behind it; up to 24 hours are kept from there. An absent series, a JSON null and
 * a physically impossible number all yield NAN (-1 for the integer indices) rather
 * than an error; only a broken document, an irregular hourly grid, a non-UTC
 * response or a missing european_aqi series is an error. Pollen is null outside
 * Europe, so pollen[] is NAN there. meta.issued_at is the hour of index 0: the
 * response carries no model issue time of its own. */

bool home_parse_air(const char *json, size_t len, home_air_t *out, int64_t now, char error[97]);
/* EEA bands 0-20, 20-40, 40-60, 60-80, 80-100, >100: 0 very good, 1 good,
 * 2 moderate, 3 poor, 4 very poor, 5 extremely poor. A band owns its upper
 * bound, so 20 is very good and 100 is very poor. Returns -1 when the index
 * is unknown (european_aqi < 0), which is not a level and must not be drawn. */
int home_air_level(int european_aqi);
#endif
