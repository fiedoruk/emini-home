#ifndef HOME_QR_H
#define HOME_QR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int x, y, size, modules, scale;
} home_qr_layout_t;
/* Creates a standard WPA/WPA2 WIFI payload, with syntax escaping. Inputs
 * remain private. False leaves output empty; caller owns every buffer. */
bool home_qr_wifi_text(const char *ssid, const char *password, char *out, size_t capacity);
/* Native B0/W1 400x300 MSB-first2bpp. Four-module quiet zone, integer scale
 * >=2, ECC>=M, versions1..10. False leaves frame/layout unchanged. */
bool home_qr_paint(uint8_t frame[30000], const char *payload, int x, int y, int width, int height,
                   home_qr_layout_t *layout);
#endif
