#include "home_air.h"
#include "cJSON.h"
#include "home_parse.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define BODY_LIMIT (128U * 1024U)
#define HOUR_LIMIT 512 /* two forecast days are 48 hours; leave room, refuse a flood */
#define MEMBER_LIMIT 64

static bool fail(char error[97], const char *message)
{
    if (error)
        snprintf(error, 97, "%s", message);
    return false;
}
static bool space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
/* Mirrors the private UTF-8 check in home_parse.c. When this parser moves into
 * home_parse.c the two should become one helper instead of two copies. */
static bool utf8_next(const unsigned char *p, size_t n, size_t *used)
{
    if (!n)
        return false;
    uint32_t cp = p[0];
    size_t k = 1;
    if (cp >= 0xC2 && cp <= 0xDF) {
        cp &= 31;
        k = 2;
    } else if (cp >= 0xE0 && cp <= 0xEF) {
        cp &= 15;
        k = 3;
    } else if (cp >= 0xF0 && cp <= 0xF4) {
        cp &= 7;
        k = 4;
    } else if (cp >= 0x80)
        return false;
    if (k > n)
        return false;
    for (size_t i = 1; i < k; ++i) {
        if ((p[i] & 0xC0) != 0x80)
            return false;
        cp = (cp << 6) | (p[i] & 63);
    }
    if ((k == 2 && cp < 0x80) || (k == 3 && cp < 0x800) || (k == 4 && cp < 0x10000) ||
        cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) || (cp & 0xFFFF) >= 0xFFFE ||
        (cp < 32 && cp != 9 && cp != 10 && cp != 13))
        return false;
    *used = k;
    return true;
}
static bool valid_body(const char *body, size_t len)
{
    if (!body || !len || len > BODY_LIMIT)
        return false;
    for (size_t pos = 0; pos < len;) {
        size_t used;
        if (!utf8_next((const unsigned char *)body + pos, len - pos, &used))
            return false;
        pos += used;
    }
    return true;
}
static const cJSON *member(const cJSON *object, const char *key)
{
    return cJSON_IsObject(object) ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
}
/* cJSON keeps the first of two equal keys, so a second "european_aqi" could hide
 * behind the one we read. Refuse duplicates, nameless members and absurd counts. */
static bool unique_members(const cJSON *object)
{
    if (!cJSON_IsObject(object))
        return false;
    unsigned count = 0;
    for (const cJSON *child = object->child; child; child = child->next) {
        if (++count > MEMBER_LIMIT || !child->string)
            return false;
        for (const cJSON *other = child->next; other; other = other->next)
            if (other->string && !strcmp(child->string, other->string))
                return false;
    }
    return true;
}
/* The hourly series for key, or NULL when the document omits it. An existing
 * member of the wrong type or length is a schema error, so *ok goes false. */
static const cJSON *series(const cJSON *hourly, const char *key, int count, bool *ok)
{
    const cJSON *node = member(hourly, key);
    if (!node)
        return NULL;
    if (!cJSON_IsArray(node) || cJSON_GetArraySize(node) != count) {
        *ok = false;
        return NULL;
    }
    return node;
}
/* One sample. An absent series, a JSON null and a number outside the physical
 * range all mean "unknown" (NAN); only a non-number entry is a schema error. */
static bool sample(const cJSON *from, int index, double min, double max, double *out)
{
    *out = NAN;
    if (!from)
        return true;
    const cJSON *value = cJSON_GetArrayItem(from, index);
    if (!value || cJSON_IsNull(value))
        return true;
    if (!cJSON_IsNumber(value))
        return false;
    double v = value->valuedouble;
    if (isfinite(v) && v >= min && v <= max)
        *out = v;
    return true;
}
static bool index_sample(const cJSON *from, int index, int16_t *out)
{
    double v;
    if (!sample(from, index, 0, 1000, &v))
        return false;
    *out = isnan(v) ? (int16_t)-1 : (int16_t)lround(v);
    return true;
}
/* Open-Meteo answers timezone=UTC with naive stamps such as "2026-09-15T08:00",
 * while home_parse_time wants seconds and a zone designator; supply both. */
static int64_t hour_time(const cJSON *node)
{
    if (!cJSON_IsString(node) || !node->valuestring)
        return -1;
    const char *text = node->valuestring;
    if (strlen(text) == 16 && text[13] == ':') {
        char stamp[24];
        snprintf(stamp, sizeof stamp, "%s:00Z", text);
        return home_parse_time(stamp);
    }
    return home_parse_time(text);
}

int home_air_level(int european_aqi)
{
    if (european_aqi < 0)
        return -1;
    static const int bands[] = {20, 40, 60, 80, 100};
    for (int i = 0; i < 5; ++i)
        if (european_aqi <= bands[i])
            return i;
    return 5;
}

bool home_parse_air(const char *json, size_t len, home_air_t *out, int64_t now, char error[97])
{
    if (!out || now <= 0 || now > INT64_C(253402300799) || !valid_body(json, len))
        return fail(error, "Invalid or oversized air JSON");
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(json, len, &end, false);
    if (!root)
        return fail(error, "Malformed air JSON");
    while (end < json + len && space((unsigned char)*end))
        ++end;
    bool ok = false;
    const char *reason = "Invalid air schema";
    home_air_t parsed = {0};
    parsed.pm2_5 = parsed.pm10 = parsed.uv_index = NAN;
    parsed.european_aqi = parsed.us_aqi = -1;
    for (unsigned i = 0; i < HOME_AIR_HOURS; ++i)
        parsed.hourly_pm2_5[i] = parsed.hourly_uv[i] = NAN;
    for (unsigned i = 0; i < HOME_POLLEN_COUNT; ++i)
        parsed.pollen[i] = NAN;
    const cJSON *hourly = member(root, "hourly"), *times = member(hourly, "time");
    const cJSON *offset = member(root, "utc_offset_seconds");
    int count = cJSON_GetArraySize(times);
    if (end != json + len || !unique_members(root) || !unique_members(hourly) ||
        !cJSON_IsArray(times) || count < 1 || count > HOUR_LIMIT)
        goto done;
    /* The stamps carry no zone, so a response in local time would be read as UTC. */
    if (offset && (!cJSON_IsNumber(offset) || offset->valuedouble != 0)) {
        reason = "Air response is not in UTC";
        goto done;
    }
    if (!member(hourly, "european_aqi")) {
        reason = "Air response has no european_aqi series";
        goto done;
    }
    bool shape = true;
    const cJSON *pm2_5 = series(hourly, "pm2_5", count, &shape);
    const cJSON *pm10 = series(hourly, "pm10", count, &shape);
    const cJSON *european = series(hourly, "european_aqi", count, &shape);
    const cJSON *american = series(hourly, "us_aqi", count, &shape);
    const cJSON *uv = series(hourly, "uv_index", count, &shape);
    const cJSON *pollen[HOME_POLLEN_COUNT];
    static const char *const pollen_keys[HOME_POLLEN_COUNT] = {"alder_pollen", "birch_pollen",
                                                               "grass_pollen", "mugwort_pollen"};
    for (unsigned i = 0; i < HOME_POLLEN_COUNT; ++i)
        pollen[i] = series(hourly, pollen_keys[i], count, &shape);
    if (!shape) {
        reason = "Air series has the wrong type or length";
        goto done;
    }
    int64_t first = 0;
    int index = -1, position = 0;
    for (const cJSON *stamp = times->child; stamp; stamp = stamp->next, ++position) {
        int64_t at = hour_time(stamp);
        if (at < 0 || at % 3600 || (position && at != first + (int64_t)position * 3600)) {
            reason = "Air hours are not a regular UTC grid";
            goto done;
        }
        if (!position)
            first = at;
        if (at <= now)
            index = position;
    }
    if (index < 0) {
        reason = "Air forecast starts after now";
        goto done;
    }
    int64_t selected = first + (int64_t)index * 3600;
    if (now - selected > 3600) {
        reason = "Air forecast has expired";
        goto done;
    }
    int hours = count - index;
    if (hours > HOME_AIR_HOURS)
        hours = HOME_AIR_HOURS;
    for (int i = 0; i < hours; ++i)
        if (!sample(pm2_5, index + i, 0, 2000, &parsed.hourly_pm2_5[i]) ||
            !sample(uv, index + i, 0, 20, &parsed.hourly_uv[i])) {
            reason = "Air value is not a number";
            goto done;
        }
    if (!sample(pm10, index, 0, 2000, &parsed.pm10) ||
        !index_sample(european, index, &parsed.european_aqi) ||
        !index_sample(american, index, &parsed.us_aqi)) {
        reason = "Air value is not a number";
        goto done;
    }
    for (unsigned i = 0; i < HOME_POLLEN_COUNT; ++i)
        if (!sample(pollen[i], index, 0, 10000, &parsed.pollen[i])) {
            reason = "Air value is not a number";
            goto done;
        }
    parsed.forecast_at = selected;
    parsed.hourly_count = (uint8_t)hours;
    parsed.pm2_5 = parsed.hourly_pm2_5[0];
    parsed.uv_index = parsed.hourly_uv[0];
    parsed.meta.valid = true;
    parsed.meta.issued_at = selected;
    *out = parsed;
    if (error)
        error[0] = '\0';
    ok = true;
done:
    cJSON_Delete(root);
    return ok ? true : fail(error, reason);
}
