#include "home_parse.h"
#include "cJSON.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BODY_LIMIT (128U * 1024U)
#define XML_DEPTH 16U
#define XML_ITEMS 128U
#define XML_NAMESPACES 48U

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
static int digit(unsigned char c)
{
    return c >= '0' && c <= '9' ? c - '0' : -1;
}
static int hex(unsigned char c)
{
    if (digit(c) >= 0)
        return digit(c);
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
static bool number(const char **at, unsigned count, int *result)
{
    int n = 0;
    for (unsigned i = 0; i < count; ++i) {
        int d = digit((unsigned char)(*at)[i]);
        if (d < 0)
            return false;
        n = n * 10 + d;
    }
    *at += count;
    *result = n;
    return true;
}
static bool take(const char **at, char c)
{
    if (**at != c)
        return false;
    ++*at;
    return true;
}
static bool leap(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
static int64_t civil_seconds(int y, int m, int d, int h, int minute, int second, int offset)
{
    static const int month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (y < 1970 || y > 9999 || m < 1 || m > 12 || d < 1 ||
        d > month_days[m - 1] + (m == 2 && leap(y)) || h > 23 || minute > 59 || second > 59)
        return -1;
    /* Gregorian civil date to Unix days; independent of process TZ/time_t. */
    int yy = y - (m <= 2), era = yy / 400, year_of_era = yy - era * 400;
    int shifted_month = m + (m > 2 ? -3 : 9);
    int day_of_year = (153 * shifted_month + 2) / 5 + d - 1;
    int day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    int64_t days = (int64_t)era * 146097 + day_of_era - 719468;
    int64_t result = days * 86400 + h * 3600 + minute * 60 + second - offset;
    return result >= 0 ? result : -1;
}
static bool timezone_offset(const char **at, int *offset)
{
    const char *p = *at;
    if (*p == 'Z' || *p == 'z') {
        *at = p + 1;
        *offset = 0;
        return true;
    }
    if (*p == '+' || *p == '-') {
        int sign = *p++ == '+' ? 1 : -1, h, m;
        if (!number(&p, 2, &h))
            return false;
        if (*p == ':')
            ++p;
        if (!number(&p, 2, &m) || h > 14 || m > 59 || (h == 14 && m))
            return false;
        *at = p;
        *offset = sign * (h * 3600 + m * 60);
        return true;
    }
    static const struct {
        const char *name;
        int seconds;
    } zones[] = {{"GMT", 0},      {"UTC", 0},      {"UT", 0},       {"EST", -18000},
                 {"EDT", -14400}, {"CST", -21600}, {"CDT", -18000}, {"MST", -25200},
                 {"MDT", -21600}, {"PST", -28800}, {"PDT", -25200}};
    for (size_t i = 0; i < sizeof zones / sizeof zones[0]; ++i) {
        size_t n = strlen(zones[i].name);
        if (strncmp(p, zones[i].name, n) == 0 && (p[n] == '\0' || space((unsigned char)p[n]))) {
            *at = p + n;
            *offset = zones[i].seconds;
            return true;
        }
    }
    return false;
}

int64_t home_parse_time(const char *text)
{
    if (!text || strlen(text) > 127)
        return -1;
    const char *p = text;
    while (space((unsigned char)*p))
        ++p;
    int y = 0, m = 0, d = 0, h = 0, minute = 0, second = 0, offset = 0;
    if (strlen(p) >= 10 && p[4] == '-') {
        if (!number(&p, 4, &y) || !take(&p, '-') || !number(&p, 2, &m) || !take(&p, '-') ||
            !number(&p, 2, &d) || (*p != 'T' && *p != 't'))
            return -1;
        ++p;
        if (!number(&p, 2, &h) || !take(&p, ':') || !number(&p, 2, &minute) || !take(&p, ':') ||
            !number(&p, 2, &second))
            return -1;
        if (*p == '.') {
            ++p;
            unsigned n = 0;
            while (digit((unsigned char)*p) >= 0 && n < 10) {
                ++p;
                ++n;
            }
            if (!n || n > 9)
                return -1;
        }
        if ((*p != 'Z' && *p != 'z' && *p != '+' && *p != '-') || !timezone_offset(&p, &offset))
            return -1;
    } else {
        if (isalpha((unsigned char)*p)) {
            static const char *weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
            bool matched = false;
            for (size_t i = 0; i < 7; ++i)
                if (!strncmp(p, weekdays[i], 3) && p[3] == ',')
                    matched = true;
            if (!matched)
                return -1;
            p += 4;
            if (!space((unsigned char)*p))
                return -1;
            while (space((unsigned char)*p))
                ++p;
        }
        if (digit((unsigned char)*p) < 0)
            return -1;
        d = *p++ - '0';
        if (digit((unsigned char)*p) >= 0)
            d = d * 10 + (*p++ - '0');
        if (!take(&p, ' '))
            return -1;
        static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
        if (strlen(p) < 4)
            return -1;
        for (int i = 0; i < 12; ++i)
            if (!strncmp(p, months + 3 * i, 3))
                m = i + 1;
        if (!m)
            return -1;
        p += 3;
        if (!take(&p, ' ') || !number(&p, 2, &y))
            return -1;
        if (digit((unsigned char)*p) >= 0) {
            int remaining;
            if (!number(&p, 2, &remaining))
                return -1;
            y = y * 100 + remaining;
        } else
            y += y >= 50 ? 1900 : 2000;
        if (!take(&p, ' ') || !number(&p, 2, &h) || !take(&p, ':') || !number(&p, 2, &minute))
            return -1;
        if (*p == ':') {
            ++p;
            if (!number(&p, 2, &second))
                return -1;
        }
        if (!take(&p, ' ') || !timezone_offset(&p, &offset))
            return -1;
    }
    while (space((unsigned char)*p))
        ++p;
    if (*p)
        return -1;
    return civil_seconds(y, m, d, h, minute, second, offset);
}

static bool utf8_next(const unsigned char *p, size_t n, size_t *used, uint32_t *code)
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
    *code = cp;
    return true;
}
static bool valid_body(const char *body, size_t len)
{
    if (!body || !len || len > BODY_LIMIT)
        return false;
    for (size_t pos = 0; pos < len;) {
        size_t used;
        uint32_t cp;
        if (!utf8_next((const unsigned char *)body + pos, len - pos, &used, &cp))
            return false;
        pos += used;
    }
    return true;
}
static bool json_shape(const char *body, size_t len)
{
    bool quoted = false, escaped = false;
    unsigned depth = 0;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)body[i];
        if (quoted) {
            if (escaped) {
                if (c == 'u' && i + 4 < len && !strncmp(body + i + 1, "0000", 4))
                    return false;
                escaped = false;
            } else if (c == '\\')
                escaped = true;
            else if (c == '"')
                quoted = false;
            else if (c < 32)
                return false;
        } else if (c == '"')
            quoted = true;
        else if (c == '{' || c == '[') {
            if (++depth > 16)
                return false;
        } else if (c == '}' || c == ']') {
            if (!depth)
                return false;
            --depth;
        }
    }
    return !quoted && !depth;
}
static const cJSON *member(const cJSON *object, const char *key)
{
    return cJSON_IsObject(object) ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
}
static const char *string_value(const cJSON *value)
{
    return cJSON_IsString(value) ? value->valuestring : NULL;
}
static const cJSON *instant(const cJSON *point)
{
    return member(member(member(point, "data"), "instant"), "details");
}
static bool metric(const cJSON *object, const char *key, double min, double max, bool required,
                   double *out)
{
    const cJSON *value = member(object, key);
    if (!value) {
        *out = NAN;
        return !required;
    }
    if (!cJSON_IsNumber(value) || !isfinite(value->valuedouble) || value->valuedouble < min ||
        value->valuedouble > max)
        return false;
    *out = value->valuedouble;
    return true;
}
static bool rain(const cJSON *point, double *out)
{
    return metric(member(member(member(point, "data"), "next_1_hours"), "details"),
                  "precipitation_amount", 0, 1000, false, out);
}
static bool unit(const cJSON *units, const char *key, const char *expected)
{
    const cJSON *value = member(units, key);
    return !value || (cJSON_IsString(value) && !strcmp(value->valuestring, expected));
}
static bool json_members(const cJSON *node, unsigned depth)
{
    if (!node || depth > 16)
        return false;
    unsigned count = 0;
    for (const cJSON *child = node->child; child; child = child->next) {
        if (++count > 512 || !json_members(child, depth + 1))
            return false;
        if (cJSON_IsObject(node)) {
            if (count > 128 || !child->string)
                return false;
            for (const cJSON *other = child->next; other; other = other->next)
                if (other->string && !strcmp(child->string, other->string))
                    return false;
        }
    }
    return true;
}

bool home_parse_weather(const char *json, size_t len, home_weather_t *out, int64_t now,
                        char error[97])
{
    if (!out || now <= 0 || now > INT64_C(253402300799) || !valid_body(json, len) ||
        !json_shape(json, len))
        return fail(error, "Invalid or oversized weather JSON");
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(json, len, &end, false);
    if (!root)
        return fail(error, "Malformed weather JSON");
    while (end < json + len && space((unsigned char)*end))
        ++end;
    bool ok = false;
    const char *reason = "Invalid weather schema";
    home_weather_t parsed = {0};
    parsed.low = parsed.high = parsed.precipitation = parsed.wind_speed = parsed.cloud_cover = NAN;
    for (unsigned i = 0; i < 12; ++i)
        parsed.hourly_temperature[i] = parsed.hourly_rain[i] = NAN;
    const cJSON *properties = member(root, "properties"), *meta = member(properties, "meta");
    const cJSON *units = member(meta, "units"), *series = member(properties, "timeseries");
    parsed.meta.issued_at = home_parse_time(string_value(member(meta, "updated_at")));
    int count = cJSON_GetArraySize(series);
    if (end != json + len || !json_members(root, 0) || !cJSON_IsArray(series) || count < 1 ||
        count > 512 || parsed.meta.issued_at < 0 || parsed.meta.issued_at > now + 300 ||
        !unit(units, "air_temperature", "celsius") || !unit(units, "wind_speed", "m/s") ||
        !unit(units, "cloud_area_fraction", "%") || !unit(units, "precipitation_amount", "mm"))
        goto done;
    int64_t previous = -1, selected_time = -1;
    unsigned hours = 0;
    double min24 = INFINITY, max24 = -INFINITY;
    for (const cJSON *point = series->child; point; point = point->next) {
        int64_t at = home_parse_time(string_value(member(point, "time")));
        double temperature;
        if (at < 0 || at <= previous ||
            !metric(instant(point), "air_temperature", -100, 70, true, &temperature)) {
            reason = "Invalid forecast time or temperature";
            goto done;
        }
        previous = at;
        if (selected_time < 0 && at >= now) {
            if (at > now + 3600) {
                reason = "No current or next hourly forecast";
                goto done;
            }
            selected_time = at;
            parsed.forecast_at = at;
            parsed.temperature = temperature;
            if (!metric(instant(point), "wind_speed", 0, 150, false, &parsed.wind_speed) ||
                !metric(instant(point), "cloud_area_fraction", 0, 100, false,
                        &parsed.cloud_cover) ||
                !rain(point, &parsed.precipitation)) {
                reason = "Invalid weather metric";
                goto done;
            }
            const cJSON *summary = member(member(member(point, "data"), "next_1_hours"), "summary");
            const cJSON *symbol_value = member(summary, "symbol_code");
            if (symbol_value) {
                const char *symbol = string_value(symbol_value);
                if (!symbol || !*symbol || strlen(symbol) >= sizeof parsed.symbol) {
                    reason = "Invalid forecast symbol";
                    goto done;
                }
                for (const char *s = symbol; *s; ++s)
                    if (!((*s >= 'a' && *s <= 'z') || digit((unsigned char)*s) >= 0 || *s == '_')) {
                        reason = "Invalid forecast symbol";
                        goto done;
                    }
                snprintf(parsed.symbol, sizeof parsed.symbol, "%s", symbol);
            }
        }
        if (selected_time >= 0 && hours < 24 && at == selected_time + (int64_t)hours * 3600) {
            if (hours < 12) {
                parsed.hourly_temperature[hours] = temperature;
                if (!rain(point, &parsed.hourly_rain[hours])) {
                    reason = "Invalid hourly precipitation";
                    goto done;
                }
                parsed.hourly_count = (uint8_t)(hours + 1);
            }
            if (temperature < min24)
                min24 = temperature;
            if (temperature > max24)
                max24 = temperature;
            ++hours;
        }
    }
    if (selected_time < 0) {
        reason = "Forecast has expired";
        goto done;
    }
    if (hours == 24) {
        parsed.low = min24;
        parsed.high = max24;
    }
    parsed.meta.valid = true;
    *out = parsed;
    if (error)
        error[0] = '\0';
    ok = true;
done:
    cJSON_Delete(root);
    return ok ? true : fail(error, reason);
}

/* Bounded XML pull scanner. It validates the whole document, including ignored
 * extensions, but only projects a small RSS2/Atom subset into application data.
 * No DTD/entity expansion, recursion, resource lookup, or HTML execution. */
typedef enum { NS_NONE, NS_ATOM, NS_OTHER } namespace_kind_t;
typedef enum {
    FIELD_NONE,
    FIELD_FEED,
    FIELD_TITLE,
    FIELD_SOURCE,
    FIELD_URL,
    FIELD_PUBLISHED,
    FIELD_UPDATED
} field_kind_t;
typedef struct {
    char prefix[33];
    namespace_kind_t kind;
} namespace_binding_t;
typedef struct {
    char name[81];
    namespace_kind_t ns;
    field_kind_t field;
    unsigned namespace_mark;
    bool skip;
} xml_node_t;
typedef struct {
    char title[1025], source[385], url[1025], published[129], updated[129];
    bool title_seen, source_seen, published_seen, updated_seen;
} feed_candidate_t;
typedef struct {
    const char *body;
    size_t len, pos;
    xml_node_t nodes[XML_DEPTH];
    unsigned depth;
    namespace_binding_t bindings[XML_NAMESPACES];
    unsigned binding_count;
    bool root_seen, root_closed, atom, in_entry, declaration_seen, channel_seen, best_dated;
    unsigned entry_depth, entries, tokens;
    char feed_title[385];
    bool feed_title_seen, first_entry;
    feed_candidate_t entry;
    home_feed_t best;
    int64_t now;
} xml_parser_t;

static size_t encode_utf8(uint32_t cp, char out[4])
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 63));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 63));
        out[2] = (char)(0x80 | (cp & 63));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 63));
    out[2] = (char)(0x80 | ((cp >> 6) & 63));
    out[3] = (char)(0x80 | (cp & 63));
    return 4;
}
static bool entity(const char *text, size_t len, size_t *used, char decoded[4], size_t *decoded_len)
{
    if (!len || text[0] != '&')
        return false;
    size_t n = 1;
    while (n < len && n < 16 && text[n] != ';')
        ++n;
    if (n >= len || text[n] != ';')
        return false;
    uint32_t cp = 0;
    if (n == 3 && !memcmp(text, "&lt", 3))
        cp = '<';
    else if (n == 3 && !memcmp(text, "&gt", 3))
        cp = '>';
    else if (n == 4 && !memcmp(text, "&amp", 4))
        cp = '&';
    else if (n == 5 && !memcmp(text, "&quot", 5))
        cp = '"';
    else if (n == 5 && !memcmp(text, "&apos", 5))
        cp = '\'';
    else if (n > 2 && text[1] == '#') {
        size_t i = 2;
        unsigned base = 10;
        if (text[i] == 'x') {
            base = 16;
            ++i;
        }
        if (i == n)
            return false;
        for (; i < n; ++i) {
            int value = base == 16 ? hex((unsigned char)text[i]) : digit((unsigned char)text[i]);
            if (value < 0 || cp > (0x10FFFFU - (unsigned)value) / base)
                return false;
            cp = cp * base + (unsigned)value;
        }
    } else
        return false;
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) || (cp & 0xFFFF) >= 0xFFFE ||
        (cp < 32 && cp != 9 && cp != 10 && cp != 13))
        return false;
    *decoded_len = encode_utf8(cp, decoded);
    *used = n + 1;
    return true;
}
static bool decode_span(const char *text, size_t len, char *out, size_t capacity, bool truncate)
{
    size_t written = out ? strlen(out) : 0;
    bool full = false;
    for (size_t i = 0; i < len;) {
        size_t consumed, bytes;
        char encoded[4];
        const char *source = text + i;
        if (text[i] == '&') {
            if (!entity(text + i, len - i, &consumed, encoded, &bytes))
                return false;
            source = encoded;
        } else {
            uint32_t cp;
            if (!utf8_next((const unsigned char *)text + i, len - i, &consumed, &cp))
                return false;
            bytes = consumed;
        }
        if (out && !full) {
            if (written + bytes >= capacity) {
                if (!truncate)
                    return false;
                full = true;
            } else {
                memcpy(out + written, source, bytes);
                written += bytes;
                out[written] = '\0';
            }
        }
        i += consumed;
    }
    return true;
}
static bool name_first(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
static bool name_rest(unsigned char c)
{
    return name_first(c) || digit(c) >= 0 || c == '-' || c == '.' || c == ':';
}
static bool xml_name(xml_parser_t *p, char out[81])
{
    size_t begin = p->pos;
    if (begin >= p->len || !name_first((unsigned char)p->body[begin]))
        return false;
    ++p->pos;
    while (p->pos < p->len && name_rest((unsigned char)p->body[p->pos]))
        ++p->pos;
    size_t n = p->pos - begin;
    if (n > 80)
        return false;
    memcpy(out, p->body + begin, n);
    out[n] = '\0';
    const char *colon = strchr(out, ':');
    return !colon ||
           (colon != out && name_first((unsigned char)colon[1]) && !strchr(colon + 1, ':'));
}
static void xml_space(xml_parser_t *p)
{
    while (p->pos < p->len && space((unsigned char)p->body[p->pos]))
        ++p->pos;
}
static const char *local_name(const char *qname)
{
    const char *colon = strchr(qname, ':');
    return colon ? colon + 1 : qname;
}
static bool resolve_namespace(const xml_parser_t *p, const char *qname, namespace_kind_t *kind)
{
    const char *colon = strchr(qname, ':');
    size_t length = colon ? (size_t)(colon - qname) : 0;
    if (length > 32)
        return false;
    if (length == 3 && !memcmp(qname, "xml", 3)) {
        *kind = NS_OTHER;
        return true;
    }
    for (unsigned i = p->binding_count; i > 0; --i) {
        const namespace_binding_t *binding = &p->bindings[i - 1];
        if (strlen(binding->prefix) == length && !memcmp(binding->prefix, qname, length)) {
            *kind = binding->kind;
            return true;
        }
    }
    if (colon)
        return false;
    *kind = NS_NONE;
    return true;
}
static bool bind_namespace(xml_parser_t *p, const char *name, const char *value)
{
    const char *prefix = name[5] == ':' ? name + 6 : "";
    if (strlen(prefix) > 32 || p->binding_count >= XML_NAMESPACES || !strcmp(prefix, "xmlns"))
        return false;
    if (!strcmp(prefix, "xml") && strcmp(value, "http://www.w3.org/XML/1998/namespace"))
        return false;
    if (*prefix && !*value)
        return false;
    namespace_binding_t *binding = &p->bindings[p->binding_count++];
    snprintf(binding->prefix, sizeof binding->prefix, "%s", prefix);
    binding->kind = !*value                                         ? NS_NONE
                    : !strcmp(value, "http://www.w3.org/2005/Atom") ? NS_ATOM
                                                                    : NS_OTHER;
    return true;
}
static bool absolute_https(const char *url)
{
    if (strncmp(url, "https://", 8) || !url[8] || strlen(url) >= HOME_FEED_URL_BYTES)
        return false;
    const char *authority = url + 8, *end = authority;
    while (*end && *end != '/' && *end != '?' && *end != '#')
        ++end;
    if (end == authority)
        return false;
    for (const char *s = url; *s; ++s)
        if ((unsigned char)*s <= 32 || (unsigned char)*s >= 127 || *s == '\\' || *s == '"' ||
            *s == '\'' || *s == '<' || *s == '>')
            return false;
    for (const char *s = authority; s < end; ++s)
        if (*s == '@' || *s == '%' || *s == ':' ||
            !(isalnum((unsigned char)*s) || *s == '-' || *s == '.'))
            return false;
    return true;
}
static bool ascii_equal(const char *s, size_t n, const char *word)
{
    if (strlen(word) != n)
        return false;
    for (size_t i = 0; i < n; ++i)
        if (tolower((unsigned char)s[i]) != (unsigned char)word[i])
            return false;
    return true;
}
static void plain_text(const char *input, char *output, size_t capacity)
{
    size_t length = strlen(input), written = 0;
    bool pending_space = false;
    int suppress = 0;
    for (size_t i = 0; i < length;) {
        if (input[i] == '<') {
            size_t end = i + 1;
            while (end < length && input[end] != '>')
                ++end;
            if (end < length) {
                size_t begin = i + 1;
                bool closing = input[begin] == '/';
                if (closing)
                    ++begin;
                size_t stop = begin;
                while (stop < end && name_rest((unsigned char)input[stop]))
                    ++stop;
                if (ascii_equal(input + begin, stop - begin, "script") ||
                    ascii_equal(input + begin, stop - begin, "style")) {
                    if (closing) {
                        if (suppress)
                            --suppress;
                    } else
                        ++suppress;
                }
                pending_space = written > 0;
                i = end + 1;
                continue;
            }
        }
        size_t used, bytes;
        uint32_t cp;
        char decoded[4];
        const char *source = input + i;
        if (input[i] == '&' && entity(input + i, length - i, &used, decoded, &bytes)) {
            size_t ignored;
            (void)utf8_next((const unsigned char *)decoded, bytes, &ignored, &cp);
            source = decoded;
        } else {
            if (!utf8_next((const unsigned char *)input + i, length - i, &used, &cp))
                break;
            bytes = used;
        }
        i += used;
        if (suppress)
            continue;
        if (cp == 32 || cp == 9 || cp == 10 || cp == 13 || cp == 0xA0) {
            pending_space = written > 0;
            continue;
        }
        if (written + bytes + (pending_space ? 1U : 0U) >= capacity)
            break;
        if (pending_space) {
            output[written++] = ' ';
            pending_space = false;
        }
        memcpy(output + written, source, bytes);
        written += bytes;
    }
    output[written] = '\0';
}
static void candidate_done(xml_parser_t *p)
{
    home_feed_t candidate = {0};
    plain_text(p->entry.title, candidate.title, sizeof candidate.title);
    plain_text(p->entry.source, candidate.source, sizeof candidate.source);
    char date[129], url[1025];
    plain_text(p->entry.published[0] ? p->entry.published : p->entry.updated, date, sizeof date);
    bool dated = date[0] != 0;
    candidate.published_at = dated ? home_parse_time(date) : 0;
    if (!candidate.title[0] || candidate.published_at < 0 || candidate.published_at > p->now + 300)
        return;
    plain_text(p->entry.url, url, sizeof url);
    if (absolute_https(url) && strlen(url) < sizeof candidate.url)
        memcpy(candidate.url, url, strlen(url) + 1);
    candidate.meta.valid = true;
    candidate.meta.issued_at = candidate.published_at;
    if (!p->best.meta.valid || (!p->first_entry && ((dated && !p->best_dated) ||
        (dated && p->best_dated && candidate.published_at > p->best.published_at)))) {
        p->best = candidate;
        p->best_dated = dated;
    }
}
static char *field_buffer(xml_parser_t *p, field_kind_t field, size_t *capacity)
{
#define FIELD_CASE(label, member)                                                                  \
    case label:                                                                                    \
        *capacity = sizeof(p->member);                                                             \
        return p->member
    switch (field) {
        FIELD_CASE(FIELD_FEED, feed_title);
        FIELD_CASE(FIELD_TITLE, entry.title);
        FIELD_CASE(FIELD_SOURCE, entry.source);
        FIELD_CASE(FIELD_URL, entry.url);
        FIELD_CASE(FIELD_PUBLISHED, entry.published);
        FIELD_CASE(FIELD_UPDATED, entry.updated);
    default:
        *capacity = 0;
        return NULL;
    }
#undef FIELD_CASE
}
static bool xml_text(xml_parser_t *p, const char *text, size_t len, bool cdata)
{
    if (!p->depth) {
        if (cdata)
            return false;
        for (size_t i = 0; i < len; ++i)
            if (!space((unsigned char)text[i]))
                return false;
        return true;
    }
    xml_node_t *node = &p->nodes[p->depth - 1];
    size_t capacity = 0;
    char *out = node->skip ? NULL : field_buffer(p, node->field, &capacity);
    if (!cdata)
        return decode_span(text, len, out, capacity, true);
    if (out) {
        size_t written = strlen(out);
        for (size_t i = 0; i < len;) {
            size_t used;
            uint32_t cp;
            if (!utf8_next((const unsigned char *)text + i, len - i, &used, &cp))
                return false;
            if (written + used >= capacity)
                break;
            memcpy(out + written, text + i, used);
            written += used;
            i += used;
        }
        out[written] = '\0';
    }
    return true;
}
static bool direct_field(xml_parser_t *p, xml_node_t *node, const char *type)
{
    const char *name = local_name(node->name);
    bool expected_ns = p->atom ? node->ns == NS_ATOM : node->ns == NS_NONE;
    if (!expected_ns)
        return true;
    if (!p->in_entry &&
        ((p->atom && p->depth == 1) ||
         (!p->atom && p->depth == 2 && !strcmp(p->nodes[1].name, "channel"))) &&
        !strcmp(name, "title")) {
        if (p->feed_title_seen)
            return false;
        p->feed_title_seen = true;
        node->field = FIELD_FEED;
    } else if (p->in_entry && p->depth == p->entry_depth) {
        if (!strcmp(name, "title")) {
            if (p->entry.title_seen)
                return false;
            p->entry.title_seen = true;
            node->field = FIELD_TITLE;
            if (p->atom && *type && strcmp(type, "text") && strcmp(type, "html") &&
                strcmp(type, "xhtml"))
                return false;
        } else if (!strcmp(name, "source") && !p->atom) {
            if (p->entry.source_seen)
                return false;
            p->entry.source_seen = true;
            node->field = FIELD_SOURCE;
        } else if ((!p->atom && !strcmp(name, "pubDate")) ||
                   (p->atom && !strcmp(name, "published"))) {
            if (p->entry.published_seen)
                return false;
            p->entry.published_seen = true;
            node->field = FIELD_PUBLISHED;
        } else if (p->atom && !strcmp(name, "updated")) {
            if (p->entry.updated_seen)
                return false;
            p->entry.updated_seen = true;
            node->field = FIELD_UPDATED;
        } else if (!p->atom && !strcmp(name, "link"))
            node->field = FIELD_URL;
    } else if (p->atom && p->in_entry && p->depth == p->entry_depth + 1 && !strcmp(name, "title") &&
               !strcmp(local_name(p->nodes[p->depth - 1].name), "source")) {
        if (p->entry.source_seen)
            return false;
        p->entry.source_seen = true;
        node->field = FIELD_SOURCE;
    }
    return true;
}
static bool xml_close(xml_parser_t *p, const char *name)
{
    if (!p->depth || strcmp(p->nodes[p->depth - 1].name, name))
        return false;
    if (p->in_entry && p->depth == p->entry_depth) {
        candidate_done(p);
        p->in_entry = false;
    }
    p->binding_count = p->nodes[p->depth - 1].namespace_mark;
    --p->depth;
    if (!p->depth)
        p->root_closed = true;
    return true;
}
static bool xml_open(xml_parser_t *p)
{
    if (p->depth >= XML_DEPTH || p->root_closed)
        return false;
    xml_node_t node = {0};
    node.namespace_mark = p->binding_count;
    if (p->depth) {
        node.field = p->nodes[p->depth - 1].field;
        node.skip = p->nodes[p->depth - 1].skip;
    }
    if (!xml_name(p, node.name))
        return false;
    char attribute_names[16][81], href[1025] = "", rel[65] = "", type[65] = "";
    unsigned attributes = 0;
    bool self_closed = false;
    while (p->pos < p->len) {
        size_t before_space = p->pos;
        xml_space(p);
        if (p->pos >= p->len)
            return false;
        char c = p->body[p->pos];
        if (c == '>') {
            ++p->pos;
            break;
        }
        if (c == '/' && p->pos + 1 < p->len && p->body[p->pos + 1] == '>') {
            p->pos += 2;
            self_closed = true;
            break;
        }
        if (before_space == p->pos || attributes >= 16)
            return false;
        char name[81], value[1025] = "";
        if (!xml_name(p, name))
            return false;
        for (unsigned i = 0; i < attributes; ++i)
            if (!strcmp(name, attribute_names[i]))
                return false;
        snprintf(attribute_names[attributes++], 81, "%s", name);
        xml_space(p);
        if (p->pos >= p->len || p->body[p->pos++] != '=')
            return false;
        xml_space(p);
        if (p->pos >= p->len || (p->body[p->pos] != '\'' && p->body[p->pos] != '"'))
            return false;
        char quote = p->body[p->pos++];
        size_t begin = p->pos;
        while (p->pos < p->len && p->body[p->pos] != quote) {
            if (p->body[p->pos] == '<')
                return false;
            ++p->pos;
        }
        if (p->pos >= p->len ||
            !decode_span(p->body + begin, p->pos - begin, value, sizeof value, false))
            return false;
        ++p->pos;
        if (!strcmp(name, "xmlns") || !strncmp(name, "xmlns:", 6)) {
            if (!bind_namespace(p, name, value))
                return false;
        } else if (!strcmp(name, "href"))
            snprintf(href, sizeof href, "%s", value);
        else if (!strcmp(name, "rel")) {
            if (strlen(value) >= sizeof rel)
                return false;
            memcpy(rel, value, strlen(value) + 1);
        } else if (!strcmp(name, "type")) {
            if (strlen(value) >= sizeof type)
                return false;
            memcpy(type, value, strlen(value) + 1);
        }
    }
    if (!p->pos || p->body[p->pos - 1] != '>' || !resolve_namespace(p, node.name, &node.ns))
        return false;
    for (unsigned i = 0; i < attributes; ++i) {
        const char *name = attribute_names[i];
        namespace_kind_t ignored;
        if (strchr(name, ':') && strncmp(name, "xmlns:", 6) &&
            !resolve_namespace(p, name, &ignored))
            return false;
    }
    const char *name = local_name(node.name);
    if (!p->depth) {
        if (p->root_seen)
            return false;
        p->root_seen = true;
        if (!strcmp(name, "feed") && node.ns == NS_ATOM)
            p->atom = true;
        else if (strcmp(name, "rss") || node.ns != NS_NONE)
            return false;
    }
    if (!p->atom && p->depth == 1 && node.ns == NS_NONE && !strcmp(name, "channel")) {
        if (p->channel_seen)
            return false;
        p->channel_seen = true;
    }
    bool is_entry = p->atom ? (p->depth == 1 && node.ns == NS_ATOM && !strcmp(name, "entry"))
                            : (p->depth == 2 && node.ns == NS_NONE && !strcmp(name, "item") &&
                               !strcmp(p->nodes[1].name, "channel"));
    if (is_entry) {
        if (p->in_entry || ++p->entries > XML_ITEMS)
            return false;
        p->in_entry = true;
        p->entry_depth = p->depth + 1;
        memset(&p->entry, 0, sizeof p->entry);
    } else if (!direct_field(p, &node, type))
        return false;
    if (p->atom && p->in_entry && p->depth == p->entry_depth && node.ns == NS_ATOM &&
        !strcmp(name, "link") && (!*rel || !strcmp(rel, "alternate")) && !p->entry.url[0] &&
        absolute_https(href))
        snprintf(p->entry.url, sizeof p->entry.url, "%s", href);
    if (ascii_equal(name, strlen(name), "script") || ascii_equal(name, strlen(name), "style"))
        node.skip = true;
    p->nodes[p->depth++] = node;
    return !self_closed || xml_close(p, node.name);
}
static const char *find_span(const char *begin, size_t len, const char *needle)
{
    size_t n = strlen(needle);
    if (n > len)
        return NULL;
    for (size_t i = 0; i <= len - n; ++i)
        if (!memcmp(begin + i, needle, n))
            return begin + i;
    return NULL;
}
static bool xml_declaration(xml_parser_t *p)
{
    size_t initial = (p->len >= 3 && !memcmp(p->body, "\xEF\xBB\xBF", 3)) ? 3U : 0U;
    if (p->declaration_seen || p->pos != initial || p->len - p->pos < 6 ||
        memcmp(p->body + p->pos, "<?xml", 5) || !space((unsigned char)p->body[p->pos + 5]))
        return false;
    p->pos += 5;
    unsigned attributes = 0;
    bool encoding = false, standalone = false;
    for (;;) {
        size_t before = p->pos;
        xml_space(p);
        if (p->pos + 1 < p->len && p->body[p->pos] == '?' && p->body[p->pos + 1] == '>') {
            if (!attributes)
                return false;
            p->pos += 2;
            p->declaration_seen = true;
            return true;
        }
        char name[81], value[33];
        if (before == p->pos || ++attributes > 3 || !xml_name(p, name))
            return false;
        xml_space(p);
        if (p->pos >= p->len || p->body[p->pos++] != '=')
            return false;
        xml_space(p);
        if (p->pos >= p->len || (p->body[p->pos] != '\'' && p->body[p->pos] != '"'))
            return false;
        char quote = p->body[p->pos++];
        size_t begin = p->pos;
        while (p->pos < p->len && p->body[p->pos] != quote)
            ++p->pos;
        size_t n = p->pos - begin;
        if (p->pos >= p->len || n >= sizeof value)
            return false;
        memcpy(value, p->body + begin, n);
        value[n] = '\0';
        ++p->pos;
        if (attributes == 1) {
            if (strcmp(name, "version") || strcmp(value, "1.0"))
                return false;
        } else if (!strcmp(name, "encoding") && !encoding && !standalone) {
            if (!ascii_equal(value, n, "utf-8"))
                return false;
            encoding = true;
        } else if (!strcmp(name, "standalone") && !standalone) {
            if (strcmp(value, "yes") && strcmp(value, "no"))
                return false;
            standalone = true;
        } else
            return false;
    }
}
static bool scan_xml(xml_parser_t *p)
{
    if (p->len >= 3 && !memcmp(p->body, "\xEF\xBB\xBF", 3))
        p->pos = 3;
    while (p->pos < p->len) {
        if (++p->tokens > 8192)
            return false;
        if (p->body[p->pos] != '<') {
            size_t begin = p->pos;
            while (p->pos < p->len && p->body[p->pos] != '<')
                ++p->pos;
            if (find_span(p->body + begin, p->pos - begin, "]]>") ||
                !xml_text(p, p->body + begin, p->pos - begin, false))
                return false;
            continue;
        }
        const char *start = p->body + p->pos;
        size_t left = p->len - p->pos;
        if (left >= 4 && !memcmp(start, "<!--", 4)) {
            const char *end = find_span(start + 4, left - 4, "-->");
            if (!end || (end > start + 4 && end[-1] == '-') ||
                find_span(start + 4, (size_t)(end - start - 4), "--"))
                return false;
            p->pos = (size_t)(end - p->body) + 3;
            continue;
        }
        if (left >= 9 && !memcmp(start, "<![CDATA[", 9)) {
            const char *end = find_span(start + 9, left - 9, "]]>");
            if (!end || !xml_text(p, start + 9, (size_t)(end - start - 9), true))
                return false;
            p->pos = (size_t)(end - p->body) + 3;
            continue;
        }
        if (left >= 2 && start[1] == '?') {
            /* Only a leading XML declaration, UTF-8 input. Other processing
             * instructions are outside this data-only parser's accepted subset. */
            if (!xml_declaration(p))
                return false;
            continue;
        }
        if (left >= 2 && start[1] == '!')
            return false;
        ++p->pos;
        if (p->pos < p->len && p->body[p->pos] == '/') {
            ++p->pos;
            char name[81];
            if (!xml_name(p, name))
                return false;
            xml_space(p);
            if (p->pos >= p->len || p->body[p->pos++] != '>' || !xml_close(p, name))
                return false;
        } else if (!xml_open(p))
            return false;
    }
    return p->root_seen && p->root_closed && !p->depth && p->best.meta.valid;
}

static bool parse_feed(const char *xml, size_t len, home_feed_t *out, int64_t now,
                       char error[97], bool first_entry)
{
    if (!out || now <= 0 || now > INT64_C(253402300799) || !valid_body(xml, len))
        return fail(error, "Invalid or oversized feed XML");
    xml_parser_t *parser = calloc(1, sizeof *parser);
    if (!parser)
        return fail(error, "Feed parser memory unavailable");
    parser->body = xml;
    parser->len = len;
    parser->now = now;
    parser->first_entry = first_entry;
    bool ok = scan_xml(parser);
    if (ok) {
        if (!parser->best.source[0])
            plain_text(parser->feed_title, parser->best.source, sizeof parser->best.source);
        if (!parser->best.source[0])
            snprintf(parser->best.source, sizeof parser->best.source, "Feed");
        *out = parser->best;
    }
    free(parser);
    if (ok) {
        if (error)
            error[0] = '\0';
        return true;
    }
    return fail(error, "Malformed feed or no valid entry");
}

bool home_parse_feed(const char *xml, size_t len, home_feed_t *out, int64_t now, char error[97])
{
    return parse_feed(xml, len, out, now, error, false);
}

bool home_parse_feed_first(const char *xml, size_t len, home_feed_t *out, int64_t now,
                           char error[97])
{
    return parse_feed(xml, len, out, now, error, true);
}
