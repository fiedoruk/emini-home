#ifndef HOME_PANEL_H
#define HOME_PANEL_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HOME_PANEL_WIDTH 400U
#define HOME_PANEL_HEIGHT 300U
#define HOME_PANEL_ROW_BYTES (HOME_PANEL_WIDTH / 4U)
#define HOME_PANEL_FRAME_BYTES (HOME_PANEL_ROW_BYTES * HOME_PANEL_HEIGHT)

/* One persistent FreeRTOS task owns all calls, never an ISR. No other user
 * may touch SPI3 or GPIO 6, 8..13. init configures transport with the rail off.
 * Repeated successful init calls by that owner are harmless. */
esp_err_t home_panel_init(void);

/* Complete synchronous full refresh and source-defined normal power-down.
 * Exactly 30000 bytes, row-major, MSB-first pairs: 0 black, 1 white, 2 yellow,
 * 3 red. The caller must keep the frame immutable until this call returns.
 * ESP_OK means SPI/observed BUSY cycle/normal power-down completed; it does
 * not prove optical appearance. Invalid arguments do not touch the panel.
 * BUSY is bounded to 120 s per stage; refresh must assert BUSY within 1 s.
 * Hardware error latches FAULT: do not retry/reset/power-cycle automatically. */
esp_err_t home_panel_show(const uint8_t *frame, size_t len);

/* Called about every 50 ms while the panel is busy. A refresh takes some 25 seconds, and the
 * task that owns the panel is the same one that watches the buttons: without this hook every
 * press made during a picture is thrown away. The hook must not touch the panel. */
void home_panel_set_idle_hook(void (*hook)(void));

/* Idempotent OFF confirmation after a successful show. Also used internally
 * at the known refresh-complete boundary. This is NOT emergency recovery:
 * it refuses fault/unknown/active states rather than guessing a shutdown. */
esp_err_t home_panel_power_off(void);

#ifdef __cplusplus
}
#endif
#endif
