/* IANA time-zone lookup. No town catalogue or reverse-geocoding database. */
#include "home_places.h"
#include "generated/home_zones.h"
#include <string.h>

static bool add_ref(cJSON *object, const char *key, const char *value)
{
    cJSON *item = cJSON_CreateStringReference(value);
    if (!item)
        return false;
    if (!cJSON_AddItemToObjectCS(object, key, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}
static const home_zone_row_t *find_zone(const char *name)
{
    if (!name)
        return NULL;
    size_t length = 0;
    while (length <= 48 && name[length])
        length++;
    if (length > 48)
        return NULL;
    size_t lo = 0, hi = home_zone_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = strcmp(home_zone_rows[mid].name, name);
        if (cmp < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < home_zone_count && !strcmp(home_zone_rows[lo].name, name) ? &home_zone_rows[lo]
                                                                          : NULL;
}
const char *home_timezone_lookup(const char *name)
{
    const home_zone_row_t *zone = find_zone(name);
    return zone ? zone->name : NULL;
}
cJSON *home_timezones_json(void)
{
    cJSON *array = cJSON_CreateArray();
    if (!array)
        return NULL;
    for (size_t i = 0; i < home_zone_count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            cJSON_Delete(array);
            return NULL;
        }
        if (!add_ref(item, "name", home_zone_rows[i].name) ||
            !add_ref(item, "label", home_zone_rows[i].label) ||
            !cJSON_AddItemToArray(array, item)) {
            cJSON_Delete(item);
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}
bool home_tz_offset_at(const char *name, int64_t utc, int32_t *offset_seconds, bool *is_dst)
{
    const home_zone_row_t *zone = find_zone(name);
    if (!zone || !offset_seconds || !is_dst || utc < HOME_ZONE_START || utc >= HOME_ZONE_STOP)
        return false;
    home_zone_span_t span = home_zone_spans[zone->span];
    size_t lo = 0, hi = span.count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (home_zone_epochs[span.start + mid] <= (uint32_t)utc)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (!lo)
        return false;
    const home_zone_state_t state = home_zone_states[home_zone_state_indices[span.start + lo - 1]];
    *offset_seconds = state.offset;
    *is_dst = state.dst != 0;
    return true;
}
bool home_tz_localtime(const char *name, int64_t utc, struct tm *out)
{
    if (!out)
        return false;
    int32_t offset;
    bool dst;
    if (!home_tz_offset_at(name, utc, &offset, &dst))
        return false;
    int64_t shifted = utc + offset;
    time_t timestamp = (time_t)shifted;
    if ((int64_t)timestamp != shifted)
        return false;
    struct tm converted;
    if (!gmtime_r(&timestamp, &converted))
        return false;
    converted.tm_isdst = dst ? 1 : 0;
    *out = converted;
    return true;
}
