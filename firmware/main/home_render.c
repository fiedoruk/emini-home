/* Home native scene renderer. Original Home implementation, 2026.
 * Exactly one pigment per pixel; generated Atkinson masks retain OFL notice.
 * Pure, reentrant C: no heap, I/O, global mutable state or device dependency. */
#include "home_types.h"
#include "home_places.h"
#include "home_qr.h"
#include "generated/home_font.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { BLACK = 0, PAPER = 1, YELLOW = 2, RED = 3, W = 400, H = 300 };
typedef struct {
    uint8_t *frame;
    int cell, intensity;
    bool pl; /* Polish line breaks keep a one-letter word with the next word. */
} canvas_t;
static const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
static int imin(int a, int b)
{
    return a < b ? a : b;
}
static int imax(int a, int b)
{
    return a > b ? a : b;
}
static double clamp(double v, double a, double b)
{
    return !isfinite(v) ? a : v < a ? a : v > b ? b : v;
}
static float clampf(float v, float a, float b)
{
    return !isfinite(v) ? a : v < a ? a : v > b ? b : v;
}
static bool polish(const home_config_t *c)
{
    return c->locale[0] == 'p' && c->locale[1] == 'l';
}
static const char *tr(bool pl, const char *en, const char *pol)
{
    return pl ? pol : en;
}
static void pixel(canvas_t *c, int x, int y, int p)
{
    if ((unsigned)x >= W || (unsigned)y >= H)
        return;
    if (c->intensity == 0) {
        if (p == YELLOW)
            p = PAPER;
        else if (p == RED)
            p = BLACK;
    }
    unsigned i = (unsigned)y * 100u + (unsigned)x / 4u, shift = 6u - ((unsigned)x % 4u) * 2u;
    c->frame[i] = (uint8_t)((c->frame[i] & ~(3u << shift)) | ((unsigned)p << shift));
}
static void rect(canvas_t *c, int x, int y, int w, int h, int p)
{
    for (int yy = imax(y, 0); yy < imin(y + h, H); ++yy)
        for (int xx = imax(x, 0); xx < imin(x + w, W); ++xx)
            pixel(c, xx, yy, p);
}
static int mix(canvas_t *c, int x, int y, int a, int b, float coverage)
{
    /* cell is1/2/4; shifting avoids two runtime divisions per pigment pixel. */
    int shift = c->cell >> 1;
    if (c->intensity == 0) {
        if (a == YELLOW)
            a = PAPER;
        if (a == RED)
            a = BLACK;
        if (b == YELLOW)
            b = PAPER;
        if (b == RED)
            b = BLACK;
    }
    /* Colour is never finer than 2 px; black-and-paper patterns keep 1 px. */
    if (shift == 0 && (a == YELLOW || a == RED || b == YELLOW || b == RED))
        shift = 1;
    float threshold = (bayer[(y >> shift) & 3][(x >> shift) & 3] + 0.5f) * 0.0625f;
    if (c->intensity == 1)
        coverage *= 0.55f;
    return coverage > threshold ? b : a;
}
static void line(canvas_t *c, int x0, int y0, int x1, int y1, int p)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1,
        err = dx + dy;
    for (;;) {
        pixel(c, x0, y0, p);
        if (x0 == x1 && y0 == y1)
            break;
        int e = 2 * err;
        if (e >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}
static size_t bounded(const char *s, size_t cap)
{
    size_t n = 0;
    if (s)
        while (n < cap && s[n])
            ++n;
    return n;
}
static bool contains(const char *s, size_t cap, const char *needle)
{
    size_t n = bounded(s, cap), m = strlen(needle);
    if (m > n)
        return false;
    for (size_t i = 0; i <= n - m; ++i)
        if (!memcmp(s + i, needle, m))
            return true;
    return false;
}
static bool time_valid(int64_t t)
{
    return t >= 1577836800LL && t <= 4102444800LL;
}
/* Consume malformed UTF-8 one byte at a time; never read beyond the span. */
static uint32_t next_cp(const char *s, size_t n, size_t *at)
{
    if (*at >= n)
        return 0;
    uint8_t a = (uint8_t)s[(*at)++];
    if (a < 0x80)
        return a;
    unsigned count = a >= 0xc2 && a <= 0xdf   ? 1
                     : a >= 0xe0 && a <= 0xef ? 2
                     : a >= 0xf0 && a <= 0xf4 ? 3
                                              : 0;
    if (!count || n - *at < count)
        return '?';
    uint32_t cp = a & ((1u << (6 - count)) - 1u);
    size_t start = *at;
    for (unsigned k = 0; k < count; ++k) {
        uint8_t b = (uint8_t)s[start + k];
        if ((b & 0xc0) != 0x80)
            return '?';
        cp = (cp << 6) | (b & 63);
    }
    if ((count == 1 && cp < 0x80) || (count == 2 && cp < 0x800) || (count == 3 && cp < 0x10000) ||
        cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        return '?';
    *at += count;
    return cp;
}
static const home_glyph_t *glyph(int fi, uint32_t cp)
{
    const home_font_t *f = &home_fonts[fi];
    unsigned lo = f->first, hi = lo + f->count;
    while (lo < hi) {
        unsigned m = lo + (hi - lo) / 2;
        if (home_glyphs[m].codepoint < cp)
            lo = m + 1;
        else
            hi = m;
    }
    if (lo < (unsigned)f->first + f->count && home_glyphs[lo].codepoint == cp)
        return &home_glyphs[lo];
    return &home_glyphs[f->first + ('?' - 32)];
}
static int width(int fi, const char *s, size_t cap)
{
    size_t at = 0, n = bounded(s, cap);
    int w = 0;
    while (at < n)
        w += glyph(fi, next_cp(s, n, &at))->advance;
    return w;
}
static void draw_glyph(canvas_t *c, int x, int baseline, int fi, uint32_t cp, int p, int bx, int by,
                       int bw, int bh)
{
    const home_glyph_t *g = glyph(fi, cp);
    int stride = (g->width + 7) / 8;
    for (int y = 0; y < g->height; ++y)
        for (int xx = 0; xx < g->width; ++xx) {
            int px = x + g->left + xx, py = baseline + g->top + y;
            if (px < bx || px >= bx + bw || py < by || py >= by + bh)
                continue;
            if (home_font_bits[g->offset + y * stride + xx / 8] & (0x80 >> (xx & 7)))
                pixel(c, px, py, p);
        }
}
/* True when cp[end] is the space after a one-letter Polish word (a i o u w z, any
 * case). Such a space is not a line break unless the line has no other space. */
static bool one_letter_word(const uint32_t *cp, int end)
{
    if (end < 1 || (end >= 2 && cp[end - 2] != ' '))
        return false;
    uint32_t ch = cp[end - 1] | 0x20u;
    return ch == 'a' || ch == 'i' || ch == 'o' || ch == 'u' || ch == 'w' || ch == 'z';
}
/* Word wrapping also breaks unspaced identifiers. Last line has a real glyph
 * ellipsis; all ink is bounded to its text region, including negative bearings. */
static void text(canvas_t *c, int x, int y, int w, int h, int fi, int p, const char *s, size_t cap)
{
    if (!s || w <= 0 || h <= 0)
        return;
    size_t n = bounded(s, cap), at = 0;
    int step = home_fonts[fi].size + 4, rows = imax(1, h / step);
    for (int row = 0; row < rows && at < n; ++row) {
        uint32_t cp[128];
        int count = 0, used = 0, space = -1, kept = -1;
        size_t space_at = 0, kept_at = 0;
        while (at < n && s[at] == ' ')
            ++at;
        size_t line_start = at;
        while (at < n && count < 128) {
            size_t before = at;
            uint32_t ch = next_cp(s, n, &at);
            if (ch == '\r')
                continue;
            if (ch == '\n')
                break;
            if (ch == '\t')
                ch = ' ';
            int advance = glyph(fi, ch)->advance;
            if (used + advance > w) {
                at = before;
                if (space < 0) {
                    space = kept;
                    space_at = kept_at;
                }
                if (space >= 0) {
                    count = space;
                    at = space_at;
                }
                break;
            }
            cp[count++] = ch;
            used += advance;
            if (ch == ' ' && c->pl && one_letter_word(cp, count - 1)) {
                kept = count - 1;
                kept_at = at;
            } else if (ch == ' ') {
                space = count - 1;
                space_at = at;
            }
        }
        if (!count && at < n &&
            at == line_start) { /* A narrower box than one glyph must still progress. */
            size_t skip = at;
            next_cp(s, n, &skip);
            at = skip;
        }
        while (count && cp[count - 1] == ' ')
            --count;
        if (row == rows - 1 && at < n) {
            int avail = w - glyph(fi, 0x2026)->advance;
            used = 0;
            int keep = 0;
            while (keep < count && used + glyph(fi, cp[keep])->advance <= avail)
                used += glyph(fi, cp[keep++])->advance;
            count = keep;
            if (count < 128)
                cp[count++] = 0x2026;
        }
        int cursor = x;
        for (int k = 0; k < count; ++k) {
            draw_glyph(c, cursor, y + row * step + home_fonts[fi].size, fi, cp[k], p, x, y, w, h);
            cursor += glyph(fi, cp[k])->advance;
        }
    }
}
static void txt(canvas_t *c, int x, int y, int w, int h, int fi, const char *s)
{
    text(c, x, y, w, h, fi, BLACK, s, 512);
}
static void top(canvas_t *c, const home_config_t *cfg, const char *section)
{
    text(c, 14, 7, 192, 20, 1, BLACK, cfg->name[0] ? cfg->name : "emini HOME", sizeof cfg->name);
    int tw = width(0, section, 96);
    txt(c, imax(212, 386 - tw), 9, 174, 17, 0, section);
    rect(c, 14, 31, 372, 1, BLACK);
}
static void stamp(char *out, size_t len, int64_t epoch, bool pl, bool clock24, const char *zone)
{
    struct tm tm;
    if (epoch <= 0 || !home_tz_localtime(zone, epoch, &tm)) {
        snprintf(out, len, "%s", tr(pl, "Date unknown", "Data nieznana"));
        return;
    }
    static const char *en[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                               "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    static const char *po[] = {"STY", "LUT", "MAR", "KWI", "MAJ", "CZE",
                               "LIP", "SIE", "WRZ", "PAŹ", "LIS", "GRU"};
    if (clock24)
        snprintf(out, len, "%02d %s · %02d:%02d", tm.tm_mday, (pl ? po : en)[tm.tm_mon], tm.tm_hour,
                 tm.tm_min);
    else
        snprintf(out, len, "%02d %s · %d:%02d %s", tm.tm_mday, (pl ? po : en)[tm.tm_mon],
                 tm.tm_hour % 12 ? tm.tm_hour % 12 : 12, tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
}
static home_source_state_t state_at(const home_source_meta_t *m, int64_t now)
{
    if (!m->valid)
        return m->state == HOME_ERROR ? HOME_ERROR : HOME_EMPTY;
    if (m->state == HOME_ERROR)
        return HOME_ERROR;
    if (!time_valid(now) || !time_valid(m->fetched_at) || m->fetched_at > now + 300)
        return HOME_STALE;
    if (m->state == HOME_STALE || (m->expires_at > 0 && now >= m->expires_at))
        return HOME_STALE;
    return HOME_READY;
}
static void source_footer(canvas_t *c, const home_config_t *cfg, const home_source_meta_t *m,
                          int64_t now, const char *source, int64_t issue)
{
    bool pl = polish(cfg);
    char date[64], status[96];
    stamp(date, sizeof date, issue, pl, cfg->clock24, cfg->timezone);
    home_source_state_t st = state_at(m, now);
    if (st == HOME_ERROR)
        snprintf(status, sizeof status, "%s",
                 tr(pl, "Connection failed · saved data", "Brak połączenia · zapisane dane"));
    else if (!time_valid(now) || !time_valid(m->fetched_at) || m->fetched_at > now + 300)
        snprintf(status, sizeof status, "%s",
                 tr(pl, "Age unknown · check time", "Wiek nieznany · sprawdź czas"));
    else if (st == HOME_STALE)
        snprintf(
            status, sizeof status, "%s",
            tr(pl, "Older data · waiting for update", "Starsze dane · czekają na aktualizację"));
    else {
        int64_t hours = (now - m->fetched_at) / 3600;
        if (hours < 1)
            snprintf(status, sizeof status, "%s",
                     tr(pl, "Checked within the hour", "Sprawdzono w ostatniej godzinie"));
        else if (hours < 48)
            snprintf(status, sizeof status,
                     pl ? "Sprawdzono %lld godz. temu" : "Checked %lld h ago", (long long)hours);
        else
            snprintf(status, sizeof status,
                     pl ? "Sprawdzono %lld dni temu" : "Checked %lld days ago",
                     (long long)(hours / 24));
    }
    rect(c, 14, 261, 372, 1, BLACK);
    txt(c, 14, 265, 215, 17, 0, source);
    txt(c, 231, 265, 155, 17, 0, date);
    txt(c, 14, 281, 372, 17, 0, status);
}
static void empty(canvas_t *c, const home_config_t *cfg, home_screen_t screen,
                  home_source_state_t st)
{
    bool pl = polish(cfg);
    const char *title = screen == HOME_WEATHER
                            ? tr(pl, "A forecast for your place.", "Prognoza dla Twojego miejsca.")
                        : screen == HOME_FEED
                            ? tr(pl, "A little room for the world.", "Trochę miejsca na świat.")
                            : tr(pl, "Make this space yours.", "To miejsce jest dla Ciebie.");
    for (int x = 280; x < 400; ++x) {
        float coverage = ((x - 280) / 120.0f) * 0.8f;
        for (int y = 43; y < 243; ++y) {
            pixel(c, x, y, mix(c, x, y, PAPER, YELLOW, coverage));
            if ((x + y / 2) % 31 == 0)
                pixel(c, x, y, mix(c, x, y, YELLOW, RED, 0.4f));
        }
    }
    txt(c, 14, 54, 268, 112, 3, title);
    const char *body =
        screen == HOME_WEATHER ? tr(pl,
                                    "Open the phone panel and use your location. The first "
                                    "forecast will appear here.",
                                    "Otwórz panel w telefonie i użyj swojej lokalizacji. Pierwsza "
                                    "prognoza pojawi się tutaj.")
        : screen == HOME_FEED ? tr(pl,
                                   "Choose an RSS or Atom source in your phone panel. One story, "
                                   "without a stream to chase.",
                                   "Wybierz źródło RSS lub Atom w panelu telefonu. Jedna "
                                   "wiadomość, bez gonienia za strumieniem.")
                              : tr(pl,
                                   "Write a message in your phone panel. A reminder, a thought, "
                                   "something worth keeping in view.",
                                   "Wpisz wiadomość w panelu telefonu. Przypomnienie, myśl, coś, "
                                   "co warto mieć na widoku.");
    txt(c, 14, 172, 260, 80, 1, body);
    rect(c, 14, 261, 372, 1, BLACK);
    txt(c, 14, 269, 372, 22, 1,
        st == HOME_ERROR ? tr(pl, "Cannot load data · check the phone panel",
                              "Nie można pobrać danych · sprawdź panel")
                         : "emini.ink/home");
}
static const char *condition(const home_weather_t *w, bool pl)
{
    if (contains(w->symbol, sizeof w->symbol, "thunder"))
        return tr(pl, "Thunderstorms", "Burze");
    if (contains(w->symbol, sizeof w->symbol, "sleet"))
        return tr(pl, "Sleet", "Śnieg z deszczem");
    if (contains(w->symbol, sizeof w->symbol, "snow"))
        return tr(pl, "Snow", "Śnieg");
    if (contains(w->symbol, sizeof w->symbol, "rain"))
        return tr(pl, "Rain ahead", "Deszcz");
    if (contains(w->symbol, sizeof w->symbol, "fog"))
        return tr(pl, "Fog", "Mgła");
    if (contains(w->symbol, sizeof w->symbol, "clearsky"))
        return tr(pl, "Clear skies", "Pogodne niebo");
    if (contains(w->symbol, sizeof w->symbol, "fair"))
        return tr(pl, "Mostly clear", "Przeważnie pogodnie");
    if (contains(w->symbol, sizeof w->symbol, "partlycloudy"))
        return tr(pl, "Partly cloudy", "Częściowe zachmurzenie");
    if (contains(w->symbol, sizeof w->symbol, "cloudy"))
        return tr(pl, "Cloud cover", "Zachmurzenie");
    return tr(pl, "Weather forecast", "Prognoza pogody");
}
/* Large text sets the condition in 30 px only when no word is wider than its box, so
 * text() never splits a word or cuts it with an ellipsis; otherwise it stays 22 px. */
static int condition_font(const home_config_t *cfg, const char *s, int w)
{
    size_t n = strlen(s), at = 0;
    int word = 0;
    if (!cfg->large_text)
        return 2;
    while (at < n) {
        uint32_t ch = next_cp(s, n, &at);
        word = ch == ' ' ? 0 : word + glyph(3, ch)->advance;
        if (word > w)
            return 2;
    }
    return 3;
}
static double temp(double c, bool f)
{
    return f ? c * 1.8 + 32 : c;
}
/* A finite, clamped value: decimal comma in Polish, U+2212 minus, never "-0". */
static void number(char *out, size_t len, double v, int decimals, bool pl)
{
    char digits[24];
    snprintf(digits, sizeof digits, "%.*f", decimals, fabs(v));
    bool zero = strspn(digits, "0.") == strlen(digits);
    char *dot = pl ? strchr(digits, '.') : NULL;
    if (dot)
        *dot = ',';
    snprintf(out, len, "%s%s", v < 0 && !zero ? "−" : "", digits);
}
/* 24-hour "18:00" (an end at midnight reads "24:00") or 12-hour "6 PM"/"6:30 PM";
 * the AM/PM mark may be left to the other end of a range. */
static void clock_text(char *out, size_t len, const struct tm *tm, bool clock24, bool end,
                       bool mark)
{
    if (clock24) {
        snprintf(out, len, "%02d:%02d", end && !tm->tm_hour && !tm->tm_min ? 24 : tm->tm_hour,
                 tm->tm_min);
        return;
    }
    int hour = tm->tm_hour % 12 ? tm->tm_hour % 12 : 12;
    const char *meridiem = !mark ? "" : tm->tm_hour < 12 ? " AM" : " PM";
    if (tm->tm_min)
        snprintf(out, len, "%d:%02d%s", hour, tm->tm_min, meridiem);
    else
        snprintf(out, len, "%d%s", hour, meridiem);
}
/* English names the precipitation after the current symbol, so the line never says
 * rain under "Snow or sleet"; Polish "Opady" covers every kind. Later hours have no
 * symbol of their own in the cache, so the current one stands in for them. */
static const char *precipitation(const home_weather_t *w, bool pl)
{
    return pl ? "Opady"
           : contains(w->symbol, sizeof w->symbol, "sleet") ? "Sleet"
           : contains(w->symbol, sizeof w->symbol, "snow")  ? "Snow"
                                                             : "Rain";
}
/* When the first rain of the parsed hourly window starts and stops. hourly_rain[k]
 * covers the hour from forecast_at + k h. The window ends at the first hour without
 * a rain amount, so nothing is claimed past the window or past missing data. */
static bool rain_outlook(char *out, size_t len, const home_config_t *cfg, const home_weather_t *w)
{
    bool pl = polish(cfg);
    const char *noun = precipitation(w, pl);
    int n = imin(w->hourly_count, 12), start = -1, stop = -1;
    for (int k = 0; k < n; ++k)
        if (!isfinite(w->hourly_rain[k]))
            n = k;
    if (n < 2 || !time_valid(w->forecast_at))
        return false;
    for (int k = 0; k < n; ++k) {
        bool wet = w->hourly_rain[k] > 0.05; /* same threshold as rain_field() */
        if (wet && start < 0)
            start = k;
        else if (!wet && start >= 0 && stop < 0)
            stop = k;
    }
    struct tm from, until;
    if (!home_tz_localtime(cfg->timezone, w->forecast_at + (int64_t)imax(start, 0) * 3600, &from) ||
        !home_tz_localtime(cfg->timezone, w->forecast_at + (int64_t)(stop < 0 ? n : stop) * 3600,
                           &until))
        return false;
    char a[16], b[16];
    clock_text(a, sizeof a, &from, cfg->clock24, false,
               stop < 0 || (from.tm_hour < 12) != (until.tm_hour < 12));
    clock_text(b, sizeof b, &until, cfg->clock24, true, true);
    if (start < 0)
        snprintf(out, len, pl ? "Bez opadów do %s" : "Dry until %s", b);
    else if (start == 0 && stop < 0)
        snprintf(out, len, pl ? "%s przez %d h" : "%s for %d h", noun, n);
    else if (start == 0)
        snprintf(out, len, pl ? "%s do %s" : "%s until %s", noun, b);
    else if (stop < 0)
        snprintf(out, len, pl ? "%s od %s" : "%s from %s", noun, a);
    else
        snprintf(out, len, "%s %s–%s", noun, a, b);
    return true;
}
/* Fallback: the precipitation amount of the current hour. */
static void rain_amount(char *out, size_t len, const home_weather_t *w, bool pl)
{
    char mm[24];
    if (isfinite(w->precipitation)) {
        number(mm, sizeof mm, clamp(w->precipitation, 0, 999), 1, pl);
        snprintf(out, len, "%s %s mm", precipitation(w, pl), mm);
    } else
        snprintf(out, len, "%s —", precipitation(w, pl));
}
static void disc(canvas_t *c, int cx, int cy, int rx, int ry, const home_weather_t *w)
{
    float cloud = (float)clamp(w->cloud_cover / 100, 0, 1),
          warm = (float)clamp((w->temperature + 15) / 55, 0, 1);
    bool night = contains(w->symbol, sizeof w->symbol, "night");
    float inv_rx = 1.0f / rx, inv_ry = 1.0f / ry;
    /* Cloud edge and horizontal shade are constant down each column. */
    for (int x = imax(0, cx - rx); x < imin(W, cx + rx + 1); ++x) {
        float dx = (x - cx) * inv_rx;
        float shade = clampf((dx + 1.0f) * 0.35f + warm * 0.3f, 0.04f, 0.96f);
        float night_shade = 0.12f + 0.35f * (dx + 1.0f);
        float edge = 0.54f - cloud * 1.05f + 0.10f * sinf((x - cx) / 15.0f);
        float obscured_shade = cloud * 0.36f;
        for (int y = imax(34, cy - ry); y < imin(255, cy + ry + 1); ++y) {
            float dy = (y - cy) * inv_ry, r = dx * dx + dy * dy;
            if (r > 1.0f)
                continue;
            int p = night ? mix(c, x, y, PAPER, BLACK, night_shade)
                          : mix(c, x, y, YELLOW, RED, shade);
            if (dx < -0.35f && r > 0.36f && ((int)(sqrtf(r) * 40.0f) % 7) == 0)
                p = night ? PAPER : YELLOW;
            if (dy > edge)
                p = mix(c, x, y, PAPER, BLACK, obscured_shade);
            pixel(c, x, y, p);
        }
    }
}
static void rain_field(canvas_t *c, const home_weather_t *w, int y0, int y1)
{
    double rain = clamp(w->precipitation, 0, 20), wind = clamp(w->wind_speed, 0, 40);
    int16_t wind_shift[H]; /* 600B, no heap; preserve exact integer rain positions. */
    for (int y = y0; y < y1; ++y)
        wind_shift[y] = (int16_t)(wind * y / 12);
    int rain_rows = (int)clamp(rain * 1.8, 1, 7);
    bool wet = rain > 0.05;
    float wave_scale = 1.0f / (46.0f + (float)wind * 2.0f);
    for (int x = 0; x < W; ++x) {
        int horizon = y0 + 5 + (int)(4.0f * sinf(x * wave_scale));
        float inv_depth = 1.0f / imax(1, y1 - horizon), warmth = (x / 399.0f) * 0.6f;
        for (int y = y0; y < y1; ++y) {
            if (y >= horizon) {
                float d = (y - horizon) * inv_depth;
                int p = mix(c, x, y, PAPER, YELLOW, d * 0.95f);
                if (p == YELLOW)
                    p = mix(c, x, y, YELLOW, RED, d * warmth);
                pixel(c, x, y, p);
            }
            if (wet && (x + wind_shift[y]) % 13 == 0 && (y - y0) % 9 < rain_rows)
                pixel(c, x, y, BLACK);
        }
    }
}
static bool graph_valid(const home_weather_t *w)
{
    int n = imin(w->hourly_count, 12);
    if (n < 2)
        return false;
    for (int k = 0; k < n; ++k)
        if (!isfinite(w->hourly_temperature[k]))
            return false;
    return true;
}
static void forecast_graph(canvas_t *c, const home_weather_t *w, int x, int y, int ww, int hh)
{
    int n = imin(w->hourly_count, 12);
    if (!graph_valid(w))
        return;
    double low = 1000, high = -1000;
    for (int k = 0; k < n; ++k) {
        double t = clamp(w->hourly_temperature[k], -100, 100);
        if (t < low)
            low = t;
        if (t > high)
            high = t;
    }
    if (high - low < 2) {
        low -= 1;
        high += 1;
    }
    int lastx = x, lasty = y + hh / 2;
    for (int xx = 0; xx < ww; ++xx) {
        double index = (double)xx * (n - 1) / (ww - 1);
        int k = imin((int)index, n - 2);
        double f = index - k;
        double value = clamp(w->hourly_temperature[k], -100, 100) * (1 - f) +
                       clamp(w->hourly_temperature[k + 1], -100, 100) * f;
        int py = y + hh - 1 - (int)((value - low) / (high - low) * (hh - 1));
        float shade_step = 0.78f / imax(1, y + hh - py);
        for (int yy = py; yy < y + hh; ++yy)
            pixel(c, x + xx, yy,
                  mix(c, x + xx, yy, YELLOW, RED, (yy - py) * shade_step));
        if (xx)
            line(c, lastx, lasty, x + xx, py, BLACK);
        lastx = x + xx;
        lasty = py;
    }
    for (int k = 0; k < n; ++k) {
        int xx = x + k * (ww - 1) / (n - 1);
        int bars = (int)clamp(w->hourly_rain[k] * 3, 0, hh / 2);
        for (int yy = y + hh - bars; yy < y + hh; ++yy)
            for (int dx = -3; dx <= 3; ++dx)
                pixel(c, xx + dx, yy, mix(c, xx + dx, yy, PAPER, BLACK, 0.72f));
    }
}
static void weather(canvas_t *c, const home_config_t *cfg, const home_weather_t *w, int64_t now)
{
    bool pl = polish(cfg), f = cfg->units[0] == 'F';
    int style = cfg->style[HOME_WEATHER] <= HOME_ATLAS ? cfg->style[HOME_WEATHER] : HOME_PRINT;
    char label[160], value[32], range[80], metrics[128], rain[64], wind[32], a[24], b[24];
    snprintf(label, sizeof label, "%.64s",
             cfg->location[0] ? cfg->location : tr(pl, "Weather", "Pogoda"));
    top(c, cfg, label);
    if (!w->meta.valid) {
        empty(c, cfg, HOME_WEATHER, w->meta.state);
        return;
    }
    if (!isfinite(w->temperature)) {
        empty(c, cfg, HOME_WEATHER, HOME_ERROR);
        return;
    }
    number(a, sizeof a, temp(clamp(w->temperature, -100, 100), f), 0, pl);
    snprintf(value, sizeof value, "%s°", a);
    if (isfinite(w->low) && isfinite(w->high)) {
        number(a, sizeof a, temp(clamp(w->low, -100, 100), f), 0, pl);
        number(b, sizeof b, temp(clamp(w->high, -100, 100), f), 0, pl);
        /* A spaced dash keeps "−7 – −2" apart from the minus signs. */
        bool negative = !strncmp(a, "−", 3) || !strncmp(b, "−", 3);
        snprintf(range, sizeof range, "%s%s%s °%s · %s", a, negative ? " – " : "–", b,
                 f ? "F" : "C", pl ? "24 h" : "24h");
    } else
        snprintf(range, sizeof range, "°%s · %s", f ? "F" : "C",
                 tr(pl, "No range", "Brak zakresu"));
    if (!rain_outlook(rain, sizeof rain, cfg, w))
        rain_amount(rain, sizeof rain, w, pl);
    if (isfinite(w->wind_speed)) {
        number(a, sizeof a, clamp(w->wind_speed, 0, 150), 1, pl);
        snprintf(wind, sizeof wind, pl ? "Wiatr %s m/s" : "Wind %s m/s", a);
    } else
        snprintf(wind, sizeof wind, pl ? "Wiatr —" : "Wind —");
    snprintf(metrics, sizeof metrics, "%s · %s", rain, wind);
    if (width(1, metrics, sizeof metrics) > 372) {
        rain_amount(rain, sizeof rain, w, pl);
        snprintf(metrics, sizeof metrics, "%s · %s", rain, wind);
    }
    if (style == HOME_RHYTHM) {
        txt(c, 14, 40, 178, 19, 0, tr(pl, "FORECAST", "PROGNOZA"));
        txt(c, 12, 56, 174, 82, width(4, value, 32) > 174 ? 3 : 4, value);
        /* Two 30 px lines with descenders need 72 px: large text starts the
         * condition level with the FORECAST label, still above the range. */
        const char *sky = condition(w, pl);
        if (condition_font(cfg, sky, 191) == 3)
            txt(c, 195, 39, 191, 74, 3, sky);
        else
            txt(c, 195, 53, 191, 60, 2, sky);
        txt(c, 195, 113, 191, 23, 1, range);
        float cloud = (float)clamp(w->cloud_cover / 100, 0, 1);
        /* The cloud texture stops where the graph ends, above the label strip. */
        for (int x = 0; x < W; ++x) {
            float coverage = cloud * (x / 400.0f) * 0.5f;
            for (int y = 141; y < 216; ++y)
                pixel(c, x, y, mix(c, x, y, PAPER, YELLOW, coverage));
        }
        forecast_graph(c, w, 14, 146, 372, 70);
        if (!graph_valid(w))
            rain_field(c, w, 146, 216);
        rect(c, 14, 219, 372, 18, PAPER);
        snprintf(label, sizeof label, pl ? "KOLEJNE GODZINY · °%s / mm" : "NEXT HOURS · °%s / mm",
                 f ? "F" : "C");
        txt(c, 14, 219, 372, 17, 0,
            graph_valid(w) ? label
                           : tr(pl, "Hourly detail unavailable", "Brak prognozy godzinowej"));
        txt(c, 14, 239, 372, 21, 1, metrics);
    } else if (style == HOME_ATLAS) {
        for (int y = 38; y < 250; ++y)
            for (int x = 0; x < 196; ++x)
                if ((x + y) % 17 < 2)
                    pixel(c, x, y, YELLOW);
        disc(c, 90, 132, 102, 99, w);
        rain_field(c, w, 218, 256);
        rect(c, 196, 35, 204, 182, PAPER);
        txt(c, 207, 44, 180, 18, 0, tr(pl, "FORECAST", "PROGNOZA"));
        /* The range sits under the reading in the paper column, so the rain
         * field under the disc runs without a hole cut for a label. Two 30 px
         * condition lines need 72 px above the reading strip at y 210, so large
         * text lifts the reading and the range by 8 px. */
        const char *sky = condition(w, pl);
        int lift = condition_font(cfg, sky, 179) == 3 ? 8 : 0;
        txt(c, 203, 53 - lift, 183, 66, width(4, value, 32) > 183 ? 3 : 4, value);
        txt(c, 208, 121 - lift, 179, 22, 1, range);
        txt(c, 208, 145 - lift, 179, lift ? 72 : 60, lift ? 3 : 2, sky);
        // Weather quantities occupy a clean reading strip above the footer. It
        // meets the range box and the rain field's last row, so nothing shows through.
        rect(c, 194, 210, 206, 46, PAPER);
        /* The 178 px column takes a long 12-hour sentence at 12 px on the same
         * baseline; only a sentence too long for that falls back to the amount. */
        if (width(1, rain, sizeof rain) <= 178)
            txt(c, 208, 214, 178, 20, 1, rain);
        else if (width(0, rain, sizeof rain) <= 178)
            txt(c, 208, 218, 178, 17, 0, rain);
        else {
            rain_amount(rain, sizeof rain, w, pl);
            txt(c, 208, 214, 178, 20, 1, rain);
        }
        txt(c, 208, 236, 178, 20, 1, wind);
    } else {
        float cloud = (float)clamp(w->cloud_cover / 100, 0, 1);
        double wind = clamp(w->wind_speed, 0, 50);
        for (int x = 184; x < W; ++x) {
            float coverage = (x - 184) / 216.0f * (0.12f + cloud * 0.4f);
            /* Keep exact engraved-line positions; only216 double sin calls
             * remain, instead of38016 per full Print weather scene. */
            int wave = (int)(wind * 2 * sin(x / 61.0));
            for (int y = 35; y < 211; ++y) {
                int p = mix(c, x, y, PAPER, YELLOW, coverage);
                if (((y + wave) % 13) == 0)
                    p = mix(c, x, y, PAPER, YELLOW, 0.72f);
                pixel(c, x, y, p);
            }
        }
        disc(c, 300, 119, 89, 78, w);
        rain_field(c, w, 218, 233);
        txt(c, 14, 40, 172, 18, 0, tr(pl, "FORECAST", "PROGNOZA"));
        /* Range under the reading, condition below it: two lines of either
         * size still end above the rain field, which keeps its whole width. */
        /* Large text needs 74 px for two 30 px lines with descenders. */
        const char *sky = condition(w, pl);
        int lift = condition_font(cfg, sky, 171) == 3 ? 6 : 0;
        txt(c, 12, 56 - lift, 174, 66, width(4, value, 32) > 174 ? 3 : 4, value);
        txt(c, 14, 124 - lift, 170, 22, 1, range);
        txt(c, 14, 148 - lift, 171, 68 + lift, lift ? 3 : 2, sky);
        txt(c, 14, 236, 372, 23, 1, metrics);
    }
    source_footer(c, cfg, &w->meta, now, "MET Norway · CC BY 4.0", w->forecast_at);
}
static uint32_t fingerprint(const char *s, size_t cap)
{
    uint32_t hash = 2166136261u;
    size_t n = bounded(s, cap);
    for (size_t i = 0; i < n; ++i)
        hash = (hash ^ (uint8_t)s[i]) * 16777619u;
    return hash;
}
/* A reproducible printed signature of this particular text, not a score or a
 * data chart. It changes when the story/message changes and exports exactly. */
typedef struct { int x, y, width, height; } paper_window_t;
static void signature(canvas_t *c, const char *s, size_t cap, int style, int topy, int bottom,
                      const paper_window_t *paper)
{
    uint32_t hash = fingerprint(s, cap);
    int extent = bottom - topy;
    float phase = (hash % 97) / 15.0f, inv_height = 1.0f / imax(1, extent - 1);
    for (int y = topy; y < bottom; ++y) {
        float a = (y - topy) * inv_height, dy = y - topy + 38.0f;
        float row_phase = phase * (13.0f / 23.0f) + a * 5.0f;
        for (int x = 0; x < W; ++x) {
            // The caller immediately paints this rectangle opaque paper.
            // Skipping its texture is bit-exact and avoids hidden trig work.
            if (paper && x >= paper->x && x < paper->x + paper->width &&
                y >= paper->y && y < paper->y + paper->height)
                continue;
            float dx = x - 200.0f;
            float pattern = style == HOME_RHYTHM ? (0.5f + 0.5f * sinf(x / 23.0f + row_phase))
                             : style == HOME_ATLAS
                                 ? (0.5f + 0.5f * cosf(sqrtf(dx * dx + dy * dy) / 13.0f + phase))
                                 : (x / 399.0f);
            pixel(c, x, y, mix(c, x, y, YELLOW, RED, pattern * a * 0.88f));
            if (style == HOME_PRINT && ((x + (hash % 11)) % 21 == 0))
                pixel(c, x, y, mix(c, x, y, PAPER, YELLOW, 0.6f));
        }
    }
}
/* Poster-only layout. Weather, setup, source labels and the existing text()
 * path are unchanged. Measure and paint share exactly the same line breaker. */
static bool poster_layout(canvas_t *c, int x, int y, int w, int h, int fi, const char *s,
                          size_t cap, bool compact, bool pl, int *height)
{
    size_t n = bounded(s, cap), at = 0;
    int row = 0, step = home_fonts[fi].size + 4, needed = 0;
    while (at < n) {
        while (at < n && (s[at] == ' ' || (compact && (s[at] == '\n' || s[at] == '\r' || s[at] == '\t'))))
            ++at;
        if (at == n)
            break;
        if (row >= 24)
            return false;
        uint32_t cp[128];
        int count = 0, used = 0, space = -1, kept = -1;
        size_t space_at = 0, kept_at = 0;
        while (at < n && count < 128) {
            size_t before = at;
            uint32_t ch = next_cp(s, n, &at);
            if (ch == '\r')
                continue;
            if (ch == '\n' && !compact)
                break;
            if (ch == '\n' || ch == '\t')
                ch = ' ';
            if (compact && ch == ' ' && (!count || cp[count - 1] == ' '))
                continue;
            const home_glyph_t *g = glyph(fi, ch);
            if (used + g->advance > w - 8 || used + g->left + g->width > w - 6) {
                at = before;
                if (!count)
                    return false;
                if (space < 0) {
                    space = kept;
                    space_at = kept_at;
                }
                if (!compact && space >= 0) {
                    count = space;
                    at = space_at;
                }
                break;
            }
            cp[count++] = ch;
            used += g->advance;
            if (ch == ' ' && pl && one_letter_word(cp, count - 1)) {
                kept = count - 1;
                kept_at = at;
            } else if (ch == ' ') {
                space = count - 1;
                space_at = at;
            }
        }
        while (count && cp[count - 1] == ' ')
            --count;
        int cursor = x + 6; /* includes the64px lowercase-j negative bearing */
        for (int i = 0; i < count; ++i) {
            const home_glyph_t *g = glyph(fi, cp[i]);
            int top = row * step + home_fonts[fi].size + g->top;
            int bottom = top + g->height;
            if (cursor + g->left < x || top < 0 || bottom > h)
                return false;
            needed = imax(needed, bottom);
            if (c)
                draw_glyph(c, cursor, y + row * step + home_fonts[fi].size, fi, cp[i], BLACK,
                           x, y, w, h);
            cursor += g->advance;
        }
        ++row;
        needed = imax(needed, row * step);
        if (needed > h)
            return false;
    }
    *height = needed;
    return true;
}
static void poster_text(canvas_t *c, int x, int y, int w, int h, const char *s, size_t cap,
                        bool large)
{
    const int candidates[] = {4, 5, 7, 3, 2, 1, 0, 6}; /* 64,48,44,30,22,16,12,10 */
    /* Each size is tried with the Polish one-letter rule first, then without it,
     * before a smaller size: a poster keeps its size rather than a perfect break. */
    for (int compact = 0; compact < 2; ++compact)
        for (unsigned i = large ? 0 : 1; i < sizeof(candidates) / sizeof(*candidates); ++i) {
            int height, fi = candidates[i];
            for (int rule = c->pl; rule >= 0; --rule) {
                if (!poster_layout(NULL, x, y, w, h, fi, s, cap, compact != 0, rule != 0, &height))
                    continue;
                int offset = (h - height) / 2;
                (void)poster_layout(c, x, y + offset, w, h - offset, fi, s, cap, compact != 0,
                                    rule != 0, &height);
                return;
            }
        }
    /* Validated EN/PL title/note byte limits fit at10px with compact wrapping.
     * Keep an explicit fallback for out-of-contract calls, never an overrun. */
    text(c, x, y, w, h, 6, BLACK, s, cap);
}
static void feed(canvas_t *c, const home_config_t *cfg, const home_feed_t *f, int64_t now)
{
    bool pl = polish(cfg);
    top(c, cfg, tr(pl, "ONE STORY", "JEDNA WIADOMOŚĆ"));
    if (!f->meta.valid || !f->title[0]) {
        empty(c, cfg, HOME_FEED, f->meta.state);
        return;
    }
    int style = cfg->style[HOME_FEED] <= HOME_ATLAS ? cfg->style[HOME_FEED] : HOME_PRINT;
    text(c, 14, 44, 372, 22, 1, BLACK,
         f->source[0] ? f->source : tr(pl, "Your source", "Twoje źródło"), sizeof f->source);
    if (style == HOME_ATLAS) {
        signature(c, f->title, sizeof f->title, style, 76, 246,
                  &(paper_window_t){14, 77, 342, 158});
        rect(c, 14, 77, 342, 158, PAPER);
        poster_text(c, 24, 81, 322, 150, f->title, sizeof f->title, cfg->large_text);
    } else if (style == HOME_RHYTHM) {
        signature(c, f->title, sizeof f->title, style, 218, 254, NULL);
        poster_text(c, 14, 79, 372, 138, f->title, sizeof f->title, cfg->large_text);
    } else {
        signature(c, f->title, sizeof f->title, style, 78, 248,
                  &(paper_window_t){0, 76, 376, 174});
        rect(c, 0, 76, 376, 174, PAPER);
        poster_text(c, 14, 81, 348, 168, f->title, sizeof f->title, cfg->large_text);
    }
    source_footer(c, cfg, &f->meta, now,
                  tr(pl, "Full story in phone panel", "Całość w panelu telefonu"), f->published_at);
}
static void note(canvas_t *c, const home_config_t *cfg, int64_t now)
{
    bool pl = polish(cfg);
    top(c, cfg, tr(pl, "YOUR NOTE", "TWOJA KARTKA"));
    if (!cfg->note[0]) {
        empty(c, cfg, HOME_NOTE, HOME_EMPTY);
        return;
    }
    int style = cfg->style[HOME_NOTE] <= HOME_ATLAS ? cfg->style[HOME_NOTE] : HOME_PRINT;
    if (style == HOME_PRINT) {
        signature(c, cfg->note, sizeof cfg->note, style, 36, 55, NULL);
        poster_text(c, 14, 68, 372, 178, cfg->note, sizeof cfg->note, cfg->large_text);
    } else if (style == HOME_RHYTHM) {
        signature(c, cfg->note, sizeof cfg->note, style, 208, 261, NULL);
        poster_text(c, 14, 52, 372, 150, cfg->note, sizeof cfg->note, cfg->large_text);
    } else {
        signature(c, cfg->note, sizeof cfg->note, style, 37, 260,
                  &(paper_window_t){14, 52, 372, 193});
        rect(c, 14, 52, 372, 193, PAPER);
        poster_text(c, 26, 61, 348, 176, cfg->note, sizeof cfg->note, cfg->large_text);
    }
    rect(c, 14, 267, 372, 1, BLACK);
    txt(c, 14, 277, 220, 19, 0, tr(pl, "Yours to keep in view.", "Warto mieć to na widoku."));
    txt(c, 270, 277, 116, 19, 0, "emini.ink/home");
    (void)now;
}
/* Card for a screen index outside weather/feed/note: the device name like every
 * other screen ("emini HOME" without one), a title, one line of help. */
static void status(canvas_t *c, const char *name, size_t cap, const char *title, const char *body)
{
    if (name && bounded(name, cap))
        text(c, 14, 8, 372, 22, 1, BLACK, name, cap);
    else
        txt(c, 14, 8, 372, 22, 1, "emini HOME");
    rect(c, 14, 35, 372, 1, BLACK);
    text(c, 14, 54, 372, 100, 3, BLACK, title ? title : "Home", 256);
    text(c, 14, 163, 372, 80, 1, BLACK, body ? body : "", 512);
    signature(c, title ? title : "Home", 256, HOME_PRINT, 251, 270, NULL);
    /* The panel is local; the web address is where help lives. */
    txt(c, 14, 279, 372, 18, 0, tr(c->pl, "Help · emini.ink/home", "Pomoc · emini.ink/home"));
}
void home_render(const home_config_t *cfg, const home_data_t *data, home_screen_t screen,
                 int64_t now, uint8_t frame[HOME_FRAME_BYTES])
{
    if (!frame)
        return;
    memset(frame, 0x55, HOME_FRAME_BYTES);
    if (!cfg || !data)
        return;
    canvas_t c = {frame, (cfg->texture == 2 || cfg->texture == 4) ? cfg->texture : 1,
                  imin(cfg->intensity, 2), polish(cfg)};
    if (screen == HOME_WEATHER && !cfg->location_ready) {
        top(&c, cfg, tr(c.pl, "Weather", "Pogoda"));
        empty(&c, cfg, HOME_WEATHER, HOME_EMPTY);
    } else if (screen == HOME_WEATHER)
        weather(&c, cfg, &data->weather, now);
    else if (screen == HOME_FEED)
        feed(&c, cfg, &data->feed, now);
    else if (screen == HOME_NOTE)
        note(&c, cfg, now);
    else {
        /* Status keeps its fixed texture and intensity, as home_render_status() does. */
        canvas_t card = {frame, 1, 2, c.pl};
        status(&card, cfg->name, sizeof cfg->name, tr(c.pl, "Choose a screen.", "Wybierz ekran."),
               tr(c.pl, "Open the panel on your phone and choose what Home shows.",
                  "Otwórz panel w telefonie i wybierz, co ma pokazywać Home."));
    }
}
void home_render_setup(const char *ssid, const char *password, const char *code,
                       const char *address, bool pl, uint8_t frame[HOME_FRAME_BYTES])
{
    if (!frame)
        return;
    memset(frame, 0x55, HOME_FRAME_BYTES);
    canvas_t c = {frame, 1, 2, pl};
    char wifi_payload[256], password_line[96];
    bool bounded_inputs = ssid && password && code && address && bounded(ssid, 33) <= 32 &&
                          bounded(password, 64) <= 63 && bounded(code, 7) == 6 &&
                          bounded(address, 128) < 128;
    if (bounded_inputs) {
        snprintf(password_line, sizeof(password_line), "%s: %s", tr(pl, "Password", "Hasło"),
                 password);
        bool fit = width(0, ssid, 33) <= 372 && width(0, password_line, sizeof(password_line)) <= 372 &&
                   width(1, address, 128) <= 372;
        bool panel_url = !strncmp(address, "http://", 7) && !strpbrk(address, "?#@\r\n");
        if (fit && panel_url && home_qr_wifi_text(ssid, password, wifi_payload, sizeof(wifi_payload)) &&
            home_qr_paint(frame, wifi_payload, 14, 48, 172, 136, NULL) &&
            home_qr_paint(frame, address, 214, 48, 172, 136, NULL)) {
            /* 26 px box: the 22 px font descends 25 rows below the box top. */
            txt(&c, 14, 3, 372, 26, 2, tr(pl, "Connect your phone.", "Połącz telefon."));
            txt(&c, 14, 29, 180, 17, 0, tr(pl, "1  JOIN WI-FI", "1  POŁĄCZ WI-FI"));
            txt(&c, 214, 29, 172, 17, 0, tr(pl, "2  OPEN HOME", "2  OTWÓRZ HOME"));
            // These values remain legible as a complete manual fallback.
            text(&c, 14, 184, 372, 17, 0, BLACK, ssid, 33);
            txt(&c, 14, 201, 372, 18, 0, password_line);
            text(&c, 14, 220, 372, 24, 1, BLACK, address, 128);
            txt(&c, 14, 253, 176, 21, 1, tr(pl, "3  Pairing code", "3  Kod parowania"));
            text(&c, 206, 246, 180, 37, 3, BLACK, code, 7);
            rect(&c, 14, 278, 372, 1, BLACK);
            txt(&c, 14, 282, 372, 17, 0, "emini.ink/home");
            // QR payload contains the private setup AP password; no token is
            // ever added to the panel URL. Frame access stays parent-private.
            memset(wifi_payload, 0, sizeof(wifi_payload));
            memset(password_line, 0, sizeof(password_line));
            return;
        }
    }
    // Unusual long fields or an encoding failure keep the complete established
    // manual setup instead of drawing a tiny/partial QR or hiding credentials.
    memset(frame, 0x55, HOME_FRAME_BYTES);
    memset(wifi_payload, 0, sizeof(wifi_payload));
    memset(password_line, 0, sizeof(password_line));
    txt(&c, 14, 7, 372, 26, 2, tr(pl, "Home, meet your phone.", "Home, poznaj swój telefon."));
    rect(&c, 14, 37, 372, 1, BLACK);
    txt(&c, 14, 42, 372, 18, 0,
        tr(pl, "1  JOIN THIS WI-FI NETWORK", "1  POŁĄCZ TELEFON Z TĄ SIECIĄ WI-FI"));
    text(&c, 14, 61, 372, 44, 1, BLACK, ssid ? ssid : "", 64);
    txt(&c, 14, 104, 372, 17, 0, tr(pl, "NETWORK PASSWORD", "HASŁO SIECI"));
    text(&c, 14, 122, 372, 60, 1, BLACK, password ? password : "", 128);
    txt(&c, 14, 185, 372, 18, 0,
        tr(pl, "2  OPEN THIS ADDRESS IN YOUR BROWSER", "2  OTWÓRZ TEN ADRES W PRZEGLĄDARCE"));
    text(&c, 14, 204, 372, 26, 2, BLACK, address ? address : "", 128);
    txt(&c, 14, 237, 160, 20, 1, tr(pl, "3  Pairing code", "3  Kod parowania"));
    text(&c, 202, 230, 184, 37, 3, BLACK, code ? code : "", 32);
    rect(&c, 14, 273, 372, 1, BLACK);
    txt(&c, 14, 279, 372, 18, 0, "emini.ink/home");
}
void home_render_status(const char *title, const char *body, bool pl,
                        uint8_t frame[HOME_FRAME_BYTES])
{
    if (!frame)
        return;
    memset(frame, 0x55, HOME_FRAME_BYTES);
    canvas_t c = {frame, 1, 2, pl};
    status(&c, NULL, 0, title, body);
}
