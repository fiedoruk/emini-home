/* Home adapter for the unmodified MIT Nayuki encoder. See LICENSE and THIRD_PARTY_NOTICES.md. */
#include "home_qr.h"
#include "qrcodegen.h"
#include <ctype.h>
#include <string.h>

static size_t length(const char *s, size_t cap)
{
    size_t n = 0;
    if (s)
        while (n < cap && s[n])
            n++;
    return n;
}
static bool append(char *out, size_t cap, size_t *n, char c)
{
    if (*n + 1 >= cap)
        return false;
    out[(*n)++] = c;
    out[*n] = 0;
    return true;
}
static bool literal(char *out, size_t cap, size_t *n, const char *s)
{
    for (; *s; s++)
        if (!append(out, cap, n, *s))
            return false;
    return true;
}
static bool field(char *out, size_t cap, size_t *n, const char *s, size_t max)
{
    size_t len = length(s, max + 1);
    if (!len || len > max)
        return false;
    bool hex = true;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 32 || c == 127)
            return false;
        if (!isxdigit(c))
            hex = false;
    }
    // Quotes disambiguate an ASCII name/password that resembles hex bytes.
    if (hex && !append(out, cap, n, '"'))
        return false;
    for (size_t i = 0; i < len; i++) {
        if (strchr("\\;,:\"", s[i]) && !append(out, cap, n, '\\'))
            return false;
        if (!append(out, cap, n, s[i]))
            return false;
    }
    return !hex || append(out, cap, n, '"');
}
bool home_qr_wifi_text(const char *ssid, const char *password, char *out, size_t cap)
{
    if (!out || !cap)
        return false;
    out[0] = 0;
    size_t n = 0;
    bool ok = ssid && password && length(password, 64) >= 8 &&
              literal(out, cap, &n, "WIFI:T:WPA;S:") && field(out, cap, &n, ssid, 32) &&
              literal(out, cap, &n, ";P:") && field(out, cap, &n, password, 63) &&
              literal(out, cap, &n, ";;");
    if (!ok)
        out[0] = 0;
    return ok;
}
static void pixel(uint8_t *frame, int x, int y, bool black)
{
    unsigned at = (unsigned)y * 100 + (unsigned)x / 4, shift = 6 - ((unsigned)x & 3) * 2;
    frame[at] = (uint8_t)((frame[at] & ~(3u << shift)) | ((black ? 0u : 1u) << shift));
}
bool home_qr_paint(uint8_t frame[30000], const char *payload, int x, int y, int width, int height,
                   home_qr_layout_t *layout)
{
    if (!frame || !payload || !payload[0] || length(payload, 257) > 256 || width < 1 ||
        height < 1 || width > 400 || height > 300 || x < 0 || y < 0 || x > 400 - width ||
        y > 300 - height)
        return false;
    uint8_t scratch[qrcodegen_BUFFER_LEN_FOR_VERSION(10)], qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    if (!qrcodegen_encodeText(payload, scratch, qr, qrcodegen_Ecc_MEDIUM, 1, 10,
                              qrcodegen_Mask_AUTO, true))
        return false;
    int modules = qrcodegen_getSize(qr), total = modules + 8,
        side = width < height ? width : height, scale = side / total;
    if (scale < 2)
        return false;
    int size = total * scale, ox = x + (width - size) / 2, oy = y + (height - size) / 2;
    for (int yy = y; yy < y + height; yy++)
        for (int xx = x; xx < x + width; xx++)
            pixel(frame, xx, yy, false);
    for (int yy = 0; yy < modules; yy++)
        for (int xx = 0; xx < modules; xx++)
            if (qrcodegen_getModule(qr, xx, yy))
                for (int dy = 0; dy < scale; dy++)
                    for (int dx = 0; dx < scale; dx++)
                        pixel(frame, ox + (xx + 4) * scale + dx, oy + (yy + 4) * scale + dy, true);
    if (layout)
        *layout = (home_qr_layout_t){ox, oy, size, modules, scale};
    return true;
}
