#include "home_location.h"
#include "home_config.h"
#include "home_places.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static bool unique_keys(const cJSON *object)
{
    for (const cJSON *a = object->child; a; a = a->next) {
        if (!a->string)
            return false;
        for (const cJSON *b = a->next; b; b = b->next)
            if (!strcmp(a->string, b->string))
                return false;
    }
    return true;
}
bool home_location_parse(const char *json, size_t size, home_area_t *out)
{
    if (!json || !out || !size || size > 16384 || memchr(json, 0, size))
        return false;
    /* cJSON uses C strings; do not accept escaped NUL truncation. */
    for (size_t i = 0; i + 6 <= size; i++)
        if (!memcmp(json + i, "\\u0000", 6))
            return false;
    const char *end = NULL;
    cJSON *j = cJSON_ParseWithLengthOpts(json, size, &end, false);
    if (!j || !cJSON_IsObject(j) || !unique_keys(j)) {
        cJSON_Delete(j);
        return false;
    }
    while (end < json + size && (*end == ' ' || *end == '\r' || *end == '\n' || *end == '\t'))
        end++;
    cJSON *lat = cJSON_GetObjectItemCaseSensitive(j, "latitude"),
          *lon = cJSON_GetObjectItemCaseSensitive(j, "longitude"),
          *city = cJSON_GetObjectItemCaseSensitive(j, "cityName"),
          *country = cJSON_GetObjectItemCaseSensitive(j, "countryCode"),
          *zones = cJSON_GetObjectItemCaseSensitive(j, "timeZones");
    bool valid = end == json + size && cJSON_IsNumber(lat) && isfinite(lat->valuedouble) &&
                 lat->valuedouble >= -90 && lat->valuedouble <= 90 && cJSON_IsNumber(lon) &&
                 isfinite(lon->valuedouble) && lon->valuedouble >= -180 &&
                 lon->valuedouble <= 180 && cJSON_IsString(city) && city->valuestring[0] &&
                 home_utf8(city->valuestring, 64, false) && cJSON_IsString(country) &&
                 strlen(country->valuestring) == 2 && country->valuestring[0] >= 'A' &&
                 country->valuestring[0] <= 'Z' && country->valuestring[1] >= 'A' &&
                 country->valuestring[1] <= 'Z';
    home_area_t area = {0};
    if (valid) {
        area.latitude = lat->valuedouble;
        area.longitude = lon->valuedouble;
        strcpy(area.city, city->valuestring);
        strcpy(area.country, country->valuestring);
        /* A single-zone country names the zone; a longer list could be any of them. */
        const cJSON *only =
            cJSON_IsArray(zones) && cJSON_GetArraySize(zones) == 1 ? zones->child : NULL;
        const char *zone = cJSON_IsString(only) ? home_timezone_lookup(only->valuestring) : NULL;
        if (zone)
            snprintf(area.timezone, sizeof(area.timezone), "%s", zone);
    }
    cJSON_Delete(j);
    if (!valid)
        return false;
    /* The phone supplies the saved time zone; area.timezone only lets the panel warn on a mismatch. */
    *out = area;
    return true;
}
bool home_location_enqueue(home_location_t *s, int64_t now, bool online, bool clock_valid,
                           bool maintenance, const char **error)
{
    const char *why = NULL;
    if (maintenance)
        why = "device_maintenance";
    else if (!online)
        why = "location_offline";
    else if (!clock_valid)
        why = "location_clock_unavailable";
    else if (s->phase == HOME_AREA_PENDING)
        why = "location_busy";
    else if (s->has_request &&
             (now < s->last_request_us || now - s->last_request_us < INT64_C(60000000)))
        why = "location_cooldown";
    if (why) {
        if (error)
            *error = why;
        return false;
    }
    s->has_request = true;
    s->requested = true;
    s->running = false;
    s->phase = HOME_AREA_PENDING;
    s->requested_us = s->last_request_us = now;
    s->generation++;
    s->error[0] = 0;
    memset(&s->area, 0, sizeof(s->area));
    if (error)
        *error = NULL;
    return true;
}
bool home_location_take(home_location_t *s, int64_t now, uint32_t *generation)
{
    if (s->phase != HOME_AREA_PENDING || !s->requested || s->running)
        return false;
    if (now < s->requested_us || now - s->requested_us > INT64_C(90000000)) {
        s->requested = false;
        s->phase = HOME_AREA_ERROR;
        strcpy(s->error, "location_expired");
        return false;
    }
    s->requested = false;
    s->running = true;
    *generation = s->generation;
    return true;
}
void home_location_expire(home_location_t *s, int64_t now)
{
    if (s->phase == HOME_AREA_PENDING && s->requested && !s->running &&
        (now < s->requested_us || now - s->requested_us > INT64_C(90000000))) {
        s->requested = false;
        s->phase = HOME_AREA_ERROR;
        strcpy(s->error, "location_expired");
    }
}
void home_location_finish(home_location_t *s, uint32_t generation, const home_area_t *area,
                          const char *error)
{
    if (s->phase != HOME_AREA_PENDING || !s->running || s->generation != generation)
        return;
    s->running = false;
    if (area) {
        s->area = *area;
        s->phase = HOME_AREA_READY;
        s->error[0] = 0;
    } else {
        s->phase = HOME_AREA_ERROR;
        snprintf(s->error, sizeof(s->error), "%s", error ? error : "location_failed");
    }
}
cJSON *home_location_json(const home_location_t *s)
{
    static const char *phases[] = {"idle", "pending", "ready", "error"};
    cJSON *j = cJSON_CreateObject();
    if (!j)
        return NULL;
    bool ok = cJSON_AddStringToObject(j, "state", phases[s->phase]) &&
              cJSON_AddStringToObject(j, "provider", HOME_LOCATION_PROVIDER) &&
              cJSON_AddStringToObject(j, "accuracy", "approximate");
    if (s->phase == HOME_AREA_READY)
        ok = ok && cJSON_AddStringToObject(j, "city", s->area.city) &&
             cJSON_AddStringToObject(j, "country", s->area.country) &&
             cJSON_AddStringToObject(j, "timezone", s->area.timezone) &&
             cJSON_AddNumberToObject(j, "latitude", s->area.latitude) &&
             cJSON_AddNumberToObject(j, "longitude", s->area.longitude);
    if (s->phase == HOME_AREA_ERROR)
        ok = ok && cJSON_AddStringToObject(j, "error", s->error);
    if (!ok) {
        cJSON_Delete(j);
        return NULL;
    }
    return j;
}
