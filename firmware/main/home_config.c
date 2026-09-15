#include "home_config.h"
#include "home_places.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *screens[] = {"weather", "feed", "note"};
static const char *styles[] = {"print", "rhythm", "atlas", "cycle"};
static const char *modes[] = {"fixed", "day", "rotate"};
const char *home_screen_name(int n)
{
    return n >= 0 && n < 3 ? screens[n] : "weather";
}
int home_screen_index(const char *s)
{
    if (s)
        for (int i = 0; i < 3; i++)
            if (!strcmp(s, screens[i]))
                return i;
    return -1;
}
const char *home_timezone(const char *name)
{
    return home_timezone_lookup(name);
}

bool home_utf8(const char *s, size_t max, bool multi)
{
    if (!s || strlen(s) > max)
        return false;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned c = *p++;
        if (c < 32) {
            if (multi && (c == 10 || c == 9))
                continue;
            return false;
        }
        if (c == 127)
            return false;
        if (c < 128)
            continue;
        unsigned need = 0, cp = 0, min = 0;
        if (c >= 0xc2 && c <= 0xdf) {
            need = 1;
            cp = c & 31;
            min = 128;
        } else if (c >= 0xe0 && c <= 0xef) {
            need = 2;
            cp = c & 15;
            min = 2048;
        } else if (c >= 0xf0 && c <= 0xf4) {
            need = 3;
            cp = c & 7;
            min = 65536;
        } else
            return false;
        while (need--) {
            if ((*p & 0xc0) != 0x80)
                return false;
            cp = (cp << 6) | (*p++ & 63);
        }
        if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
            return false;
    }
    return true;
}
void home_config_defaults(home_config_t *c)
{
    memset(c, 0, sizeof(*c));
    c->revision = 1;
    strcpy(c->name, "Home");
    strcpy(c->locale, "en");
    strcpy(c->units, "C");
    strcpy(c->timezone, "UTC");
    strcpy(c->location, "My location");
    strcpy(c->feed_url, "https://feeds.bbci.co.uk/news/world/rss.xml");
    c->location_ready = false;
    for (int i = 0; i < 3; i++) {
        c->enabled[i] = true;
        c->order[i] = i;
        c->style[i] = i;
    }
    c->texture = 1;
    c->intensity = 2;
    c->clock24 = true;
    c->mode = HOME_DAY;
    c->interval_min = 30;
    c->pause_min = 60;
    c->cycle_min = 30;
    c->ok_action = 0;
    c->quiet_enabled = true;
    c->quiet_start = 1350;
    c->quiet_end = 420;
    c->weekdays = 127;
    c->day_minute[0] = 420;
    c->day_minute[1] = 780;
    c->day_minute[2] = 1080;
    c->day_screen[0] = 0;
    c->day_screen[1] = 2;
    c->day_screen[2] = 1;
}
static bool unique(const cJSON *n, int depth)
{
    if (depth > 12)
        return false;
    if (cJSON_IsObject(n))
        for (const cJSON *a = n->child; a; a = a->next) {
            if (!a->string)
                return false;
            for (const cJSON *b = a->next; b; b = b->next)
                if (!strcmp(a->string, b->string))
                    return false;
        }
    for (const cJSON *a = n->child; a; a = a->next)
        if (!unique(a, depth + 1))
            return false;
    return true;
}
static bool known(const cJSON *obj, const char *const *keys, size_t n)
{
    if (!cJSON_IsObject(obj))
        return false;
    for (const cJSON *v = obj->child; v; v = v->next) {
        bool found = false;
        for (size_t i = 0; i < n; i++)
            if (!strcmp(v->string, keys[i]))
                found = true;
        if (!found)
            return false;
    }
    return true;
}
static cJSON *get(const cJSON *j, const char *k)
{
    return cJSON_GetObjectItemCaseSensitive(j, k);
}
static bool strval(const cJSON *j, const char *k, char *out, size_t size, bool multiline)
{
    cJSON *v = get(j, k);
    if (!cJSON_IsString(v) || !home_utf8(v->valuestring, size - 1, multiline))
        return false;
    strcpy(out, v->valuestring);
    return true;
}
static bool number(const cJSON *j, const char *k, double lo, double hi, double *out)
{
    cJSON *v = get(j, k);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble < lo ||
        v->valuedouble > hi)
        return false;
    *out = v->valuedouble;
    return true;
}
static bool integer(const cJSON *j, const char *k, int lo, int hi, int *out)
{
    double n;
    if (!number(j, k, lo, hi, &n) || floor(n) != n)
        return false;
    *out = (int)n;
    return true;
}
static bool boolean(const cJSON *j, const char *k, bool *out)
{
    cJSON *v = get(j, k);
    if (!cJSON_IsBool(v))
        return false;
    *out = cJSON_IsTrue(v);
    return true;
}
/* Short OK/BOOT press: what it does (index = home_config_t.ok_action). */
static const char *const ok_actions[] = {"language", "refresh", "hold", "setup"};
static int choice(const cJSON *j, const char *k, const char *const *values, int count)
{
    cJSON *v = get(j, k);
    if (!cJSON_IsString(v))
        return -1;
    for (int i = 0; i < count; i++)
        if (!strcmp(v->valuestring, values[i]))
            return i;
    return -1;
}
static bool hm(const cJSON *j, const char *k, uint16_t *out)
{
    cJSON *v = get(j, k);
    if (!cJSON_IsString(v))
        return false;
    const char *s = v->valuestring;
    if (strlen(s) != 5 || s[2] != ':' || !isdigit((unsigned char)s[0]) ||
        !isdigit((unsigned char)s[1]) || !isdigit((unsigned char)s[3]) ||
        !isdigit((unsigned char)s[4]))
        return false;
    int h = (s[0] - 48) * 10 + s[1] - 48, m = (s[3] - 48) * 10 + s[4] - 48;
    if (h > 23 || m > 59)
        return false;
    *out = h * 60 + m;
    return true;
}

bool home_config_decode(const char *text, size_t len, home_config_t *out, const home_config_t *base,
                        bool recipe, char err[128])
{
    const char *reason = "Invalid configuration";
    cJSON *j = NULL;
    home_config_t c;
    int n;
    static const char *const normal[] = {
        "schema",   "revision", "name",         "locale",       "units",          "timezone",
        "location", "latitude", "longitude",    "note",         "location_ready", "feed_url",
        "enabled",  "order",    "styles",       "texture",      "intensity",      "large_text",
        "clock24",  "mode",     "fixed_screen", "interval_min", "pause_min",      "quiet",
        "weekdays", "day",      "cycle_min",    "ok_action"};
    static const char *const public_keys[] = {
        "schema",   "locale",       "units",        "enabled",    "order",
        "styles",   "texture",      "intensity",    "large_text", "clock24",
        "mode",     "fixed_screen", "interval_min", "pause_min",  "quiet",
        "weekdays", "day",          "cycle_min",    "ok_action"};
#define REQUIRE(condition, why)                                                                    \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            reason = (why);                                                                        \
            goto fail;                                                                             \
        }                                                                                          \
    } while (0)
    REQUIRE(text && len > 0 && len <= 8192 && strlen(text) == len && !strstr(text, "\\u0000"),
            "Invalid body length or NUL");
    j = cJSON_ParseWithLengthOpts(text, len + 1, NULL, true);
    REQUIRE(j && unique(j, 0), "Invalid or duplicate JSON fields");
    REQUIRE(known(j, recipe ? public_keys : normal,
                  recipe ? sizeof(public_keys) / sizeof(*public_keys)
                         : sizeof(normal) / sizeof(*normal)),
            "Unknown fields");
    if (base)
        c = *base;
    else
        home_config_defaults(&c);
    REQUIRE(integer(j, "schema", 1, 1, &n), "Unsupported schema");
    if (!recipe) {
        REQUIRE(integer(j, "revision", 1, 2147483646, &n), "Invalid revision");
        c.revision = (uint32_t)n;
        REQUIRE(strval(j, "name", c.name, sizeof(c.name), false) && c.name[0],
                "Invalid device name");
        REQUIRE(strval(j, "timezone", c.timezone, sizeof(c.timezone), false) &&
                    home_timezone(c.timezone),
                "Unsupported timezone");
        REQUIRE(strval(j, "location", c.location, sizeof(c.location), false) && c.location[0],
                "Invalid place name");
        REQUIRE(number(j, "latitude", -90, 90, &c.latitude) &&
                    number(j, "longitude", -180, 180, &c.longitude),
                "Invalid coordinates");
        cJSON *location_ready = get(j, "location_ready");
        REQUIRE(!location_ready || cJSON_IsBool(location_ready), "Invalid location state");
        /* Older saved configurations already had a selected place. */
        c.location_ready = !location_ready || cJSON_IsTrue(location_ready);
        REQUIRE(strval(j, "note", c.note, sizeof(c.note), true), "Note exceeds 240 UTF-8 bytes");
        REQUIRE(strval(j, "feed_url", c.feed_url, sizeof(c.feed_url), false), "Invalid feed URL");
        if (c.feed_url[0])
            REQUIRE(!strncmp(c.feed_url, "https://", 8) && strlen(c.feed_url) > 8 &&
                        !strchr(c.feed_url, '@') && !strchr(c.feed_url, '#') &&
                        !strchr(c.feed_url, ' ') && !strchr(c.feed_url, '\\'),
                    "Feed must use public HTTPS");
    }
    REQUIRE(strval(j, "locale", c.locale, sizeof(c.locale), false) &&
                (!strcmp(c.locale, "en") || !strcmp(c.locale, "pl")),
            "Unsupported language");
    REQUIRE(strval(j, "units", c.units, sizeof(c.units), false) &&
                (!strcmp(c.units, "C") || !strcmp(c.units, "F")),
            "Unsupported units");
    cJSON *a = get(j, "enabled"), *o = get(j, "order");
    bool any = false;
    unsigned seen = 0;
    REQUIRE(cJSON_IsArray(a) && cJSON_GetArraySize(a) == 3 && cJSON_IsArray(o) &&
                cJSON_GetArraySize(o) == 3,
            "Expected three screens");
    for (int i = 0; i < 3; i++) {
        cJSON *v = cJSON_GetArrayItem(a, i);
        REQUIRE(cJSON_IsBool(v), "Invalid enabled screens");
        c.enabled[i] = cJSON_IsTrue(v);
        any |= c.enabled[i];
        v = cJSON_GetArrayItem(o, i);
        n = cJSON_IsString(v) ? home_screen_index(v->valuestring) : -1;
        REQUIRE(n >= 0 && !(seen & (1U << n)), "Invalid screen order");
        seen |= 1U << n;
        c.order[i] = n;
    }
    REQUIRE(any, "Enable at least one screen");
    cJSON *st = get(j, "styles");
    REQUIRE(known(st, screens, 3), "Invalid styles");
    for (int i = 0; i < 3; i++) {
        n = choice(st, screens[i], styles, 4);
        REQUIRE(n >= 0, "Unknown style");
        c.style[i] = n;
    }
    REQUIRE(integer(j, "texture", 1, 4, &n) && (n == 1 || n == 2 || n == 4),
            "Texture must be 1, 2 or 4");
    c.texture = n;
    REQUIRE(integer(j, "intensity", 0, 2, &n), "Invalid intensity");
    c.intensity = n;
    REQUIRE(boolean(j, "large_text", &c.large_text) && boolean(j, "clock24", &c.clock24),
            "Invalid display options");
    n = choice(j, "mode", modes, 3);
    REQUIRE(n >= 0, "Invalid mode");
    c.mode = n;
    n = choice(j, "fixed_screen", screens, 3);
    REQUIRE(n >= 0, "Invalid fixed screen");
    c.fixed_screen = n;
    REQUIRE(integer(j, "interval_min", 5, 1440, &n), "Rotation minimum is five minutes");
    c.interval_min = n;
    REQUIRE(integer(j, "pause_min", 0, 240, &n), "Invalid manual pause");
    c.pause_min = n;
    /* Optional: settings saved before 0.4.0 keep the default. */
    if (get(j, "cycle_min")) {
        REQUIRE(integer(j, "cycle_min", 5, 1440, &n), "Composition minimum is five minutes");
        c.cycle_min = n;
    }
    if (get(j, "ok_action")) { /* optional since 0.4.4 */
        n = choice(j, "ok_action", ok_actions, 4);
        REQUIRE(n >= 0, "Invalid OK button action");
        c.ok_action = (uint8_t)n;
    }
    REQUIRE(integer(j, "weekdays", 1, 127, &n), "Select at least one weekday");
    c.weekdays = n;
    cJSON *q = get(j, "quiet");
    static const char *const qkeys[] = {"enabled", "start", "end"};
    REQUIRE(known(q, qkeys, 3) && boolean(q, "enabled", &c.quiet_enabled) &&
                hm(q, "start", &c.quiet_start) && hm(q, "end", &c.quiet_end),
            "Invalid quiet hours");
    REQUIRE(!c.quiet_enabled || c.quiet_start != c.quiet_end,
            "Quiet hours must have different endpoints");
    cJSON *d = get(j, "day");
    REQUIRE(cJSON_IsArray(d) && cJSON_GetArraySize(d) == 3, "Expected three day slots");
    static const char *const dkeys[] = {"time", "screen"};
    for (int i = 0; i < 3; i++) {
        cJSON *v = cJSON_GetArrayItem(d, i);
        REQUIRE(known(v, dkeys, 2) && hm(v, "time", &c.day_minute[i]), "Invalid day slot");
        n = choice(v, "screen", screens, 3);
        REQUIRE(n >= 0, "Invalid day screen");
        c.day_screen[i] = n;
        if (i)
            REQUIRE(c.day_minute[i] > c.day_minute[i - 1], "Day times must increase");
    }
    cJSON_Delete(j);
    *out = c;
    err[0] = 0;
    return true;
fail:
    if (j)
        cJSON_Delete(j);
    snprintf(err, 128, "%s", reason);
    return false;
#undef REQUIRE
}
static void addhm(cJSON *j, const char *k, int m)
{
    if (m < 0 || m >= 1440)
        m = 0;
    int hour = m / 60, minute = m % 60;
    char b[6] = {(char)('0' + hour / 10),   (char)('0' + hour % 10),   ':',
                 (char)('0' + minute / 10), (char)('0' + minute % 10), 0};
    cJSON_AddStringToObject(j, k, b);
}
cJSON *home_config_json(const home_config_t *c, bool recipe)
{
    if (!c)
        return NULL;
    cJSON *j = cJSON_CreateObject();
    if (!j)
        return NULL;
    /* A partial object must never be persisted as a valid configuration. */
#define JSON_NEED(value)                                                                           \
    do {                                                                                           \
        if (!(value))                                                                              \
            goto oom;                                                                              \
    } while (0)
#define JSON_APPEND(array, value)                                                                  \
    do {                                                                                           \
        cJSON *child_ = (value);                                                                   \
        if (!child_)                                                                               \
            goto oom;                                                                              \
        if (!cJSON_AddItemToArray((array), child_)) {                                              \
            cJSON_Delete(child_);                                                                  \
            goto oom;                                                                              \
        }                                                                                          \
    } while (0)
    JSON_NEED(cJSON_AddNumberToObject(j, "schema", 1));
    if (!recipe) {
        JSON_NEED(cJSON_AddNumberToObject(j, "revision", c->revision));
        JSON_NEED(cJSON_AddStringToObject(j, "name", c->name));
        JSON_NEED(cJSON_AddStringToObject(j, "timezone", c->timezone));
        JSON_NEED(cJSON_AddStringToObject(j, "location", c->location));
        JSON_NEED(cJSON_AddNumberToObject(j, "latitude", c->latitude));
        JSON_NEED(cJSON_AddNumberToObject(j, "longitude", c->longitude));
        JSON_NEED(cJSON_AddBoolToObject(j, "location_ready", c->location_ready));
        JSON_NEED(cJSON_AddStringToObject(j, "note", c->note));
        JSON_NEED(cJSON_AddStringToObject(j, "feed_url", c->feed_url));
    }
    JSON_NEED(cJSON_AddStringToObject(j, "locale", c->locale));
    JSON_NEED(cJSON_AddStringToObject(j, "units", c->units));
    cJSON *a = cJSON_AddArrayToObject(j, "enabled");
    JSON_NEED(a);
    cJSON *o = cJSON_AddArrayToObject(j, "order");
    JSON_NEED(o);
    cJSON *s = cJSON_AddObjectToObject(j, "styles");
    JSON_NEED(s);
    for (int i = 0; i < 3; i++) {
        JSON_APPEND(a, cJSON_CreateBool(c->enabled[i]));
        JSON_APPEND(o, cJSON_CreateString(screens[c->order[i]]));
        JSON_NEED(cJSON_AddStringToObject(s, screens[i], styles[c->style[i]]));
    }
    JSON_NEED(cJSON_AddNumberToObject(j, "texture", c->texture));
    JSON_NEED(cJSON_AddNumberToObject(j, "intensity", c->intensity));
    JSON_NEED(cJSON_AddBoolToObject(j, "large_text", c->large_text));
    JSON_NEED(cJSON_AddBoolToObject(j, "clock24", c->clock24));
    JSON_NEED(cJSON_AddStringToObject(j, "mode", modes[c->mode]));
    JSON_NEED(cJSON_AddStringToObject(j, "fixed_screen", screens[c->fixed_screen]));
    JSON_NEED(cJSON_AddNumberToObject(j, "interval_min", c->interval_min));
    JSON_NEED(cJSON_AddNumberToObject(j, "pause_min", c->pause_min));
    JSON_NEED(cJSON_AddNumberToObject(j, "cycle_min", c->cycle_min));
    JSON_NEED(
        cJSON_AddStringToObject(j, "ok_action", ok_actions[c->ok_action < 4 ? c->ok_action : 0]));
    JSON_NEED(cJSON_AddNumberToObject(j, "weekdays", c->weekdays));
    cJSON *q = cJSON_AddObjectToObject(j, "quiet");
    JSON_NEED(q);
    JSON_NEED(cJSON_AddBoolToObject(q, "enabled", c->quiet_enabled));
    addhm(q, "start", c->quiet_start);
    JSON_NEED(get(q, "start"));
    addhm(q, "end", c->quiet_end);
    JSON_NEED(get(q, "end"));
    cJSON *d = cJSON_AddArrayToObject(j, "day");
    JSON_NEED(d);
    for (int i = 0; i < 3; i++) {
        cJSON *v = cJSON_CreateObject();
        JSON_APPEND(d, v);
        addhm(v, "time", c->day_minute[i]);
        JSON_NEED(get(v, "time"));
        JSON_NEED(cJSON_AddStringToObject(v, "screen", screens[c->day_screen[i]]));
    }
    return j;
oom:
    cJSON_Delete(j);
    return NULL;
#undef JSON_APPEND
#undef JSON_NEED
}
bool home_is_quiet(const home_config_t *c, const struct tm *t)
{
    if (!c->quiet_enabled || !t)
        return false;
    int m = t->tm_hour * 60 + t->tm_min;
    return c->quiet_start < c->quiet_end ? (m >= c->quiet_start && m < c->quiet_end)
                                         : (m >= c->quiet_start || m < c->quiet_end);
}
int home_schedule_screen(const home_config_t *c, const struct tm *t, int current, int64_t elapsed)
{
    if (c->mode != HOME_FIXED && t && !(c->weekdays & (1 << ((t->tm_wday + 6) % 7))))
        return current;
    int desired = current;
    if (c->mode == HOME_FIXED)
        desired = c->fixed_screen;
    else if (c->mode == HOME_DAY && t) {
        int dow = (t->tm_wday + 6) % 7;
        if (!(c->weekdays & (1 << dow)))
            return current;
        int m = t->tm_hour * 60 + t->tm_min;
        desired = c->day_screen[2];
        for (int i = 0; i < 3; i++)
            if (m >= c->day_minute[i])
                desired = c->day_screen[i];
    } else if (c->mode == HOME_ROTATE && elapsed >= c->interval_min * 60) {
        for (int i = 0; i < 3; i++)
            if (c->order[i] == current) {
                for (int n = 1; n <= 3; n++) {
                    int j = c->order[(i + n) % 3];
                    if (c->enabled[j])
                        return j;
                }
            }
    }
    if (desired >= 0 && desired < 3 && c->enabled[desired])
        return desired;
    for (int i = 0; i < 3; i++)
        if (c->enabled[c->order[i]])
            return c->order[i];
    return current;
}

int home_style_for(const home_config_t *c, int screen, uint32_t showing)
{
    if (screen < 0 || screen > 2)
        return HOME_PRINT;
    int style = c->style[screen];
    if (style != HOME_CYCLE)
        return style <= HOME_ATLAS ? style : HOME_PRINT;
    return showing ? (int)((showing - 1) % 3) : HOME_PRINT;
}
int home_auto_screen(const home_config_t *c, const home_data_t *d, const struct tm *t, int current,
                     int64_t elapsed)
{
    /* Without local time, weekday and quiet-hour decisions are unknown.
     * Manual requests bypass this automatic selector in the main loop. */
    if (!t && c->mode != HOME_FIXED)
        return current;
    home_config_t ready = *c;
    ready.enabled[HOME_WEATHER] &= c->location_ready && d->weather.meta.valid;
    ready.enabled[HOME_FEED] &= d->feed.meta.valid;
    ready.enabled[HOME_NOTE] &= c->note[0] != 0;
    if (!ready.enabled[0] && !ready.enabled[1] && !ready.enabled[2])
        return current;
    return home_schedule_screen(&ready, t, current, elapsed);
}
