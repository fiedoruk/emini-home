#ifndef HOME_RUNTIME_H
#define HOME_RUNTIME_H
#include "home_types.h"
#include "cJSON.h"
#include "home_store.h"
#include "home_battery.h"
#include "home_location.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include <time.h>

typedef struct {
    SemaphoreHandle_t lock;
    home_config_t config;
    home_data_t data;
    home_secrets_t secrets;
    home_battery_t battery;
    bool online, time_valid, setup, frame_valid, dirty, wifi_pending, maintenance, source_active;
    /* A press on the device is answered on the device: the next picture goes to the panel even
     * when it comes out identical, so a deliberate refresh is never silent. */
    bool force_show;
    int phase; /* 0ready,1preparing,2refreshing,3error */
    int displayed_screen, pending_screen;
    uint32_t generation, refresh_ms, render_ms;
    uint64_t request_id, manual_id;
    bool pending_manual;
    int64_t manual_until, last_switch, pair_until, info_until;
    /* Breath mode: the radio stays on and the panel answers until then (a press of OK, a request). */
    int64_t awake_until;
    home_counters_t counters;
    home_power_log_t power_log;
    uint8_t refresh_requested;
    unsigned api_active;
    /* The last gesture the device recognised, so a press can be checked without a cable. */
    int key_last, key_last_ms;
    const char *key_last_what;
    char address[32], hostname[40], ssid[33], pair_code[7];
    uint8_t *frame;
} home_runtime_t;
extern home_runtime_t home_runtime;
void home_lock(void);
void home_unlock(void);
void home_begin_pairing(void);
void home_stats_snapshot(home_stats_t *out, int64_t now);
/* The main loop, split so the host simulation can drive it: begin() takes the buffers and
 * resets what one run remembers, step() is one turn. app_main does nothing else but wait
 * twenty milliseconds between turns. */
void home_locks_init(void);
void home_loop_begin(home_config_t *c, home_data_t *d, uint8_t *work);
void home_loop_step(void);
int home_loop_wait_ms(void);
void home_loop_wait(void);
/* How many times the loop has woken since boot - in /api/status, as evidence that the chip really sleeps. */
uint32_t home_loop_wakes(void);
esp_err_t home_network_start(void);
void home_network_apply(void);
int64_t home_network_radio_seconds(void);
bool home_network_radio_on(void);
esp_err_t home_network_credentials(const char *ssid, const char *password);
esp_err_t home_network_scan_start(void);
cJSON *home_network_scan_json(void);
cJSON *home_network_status_json(void);
esp_err_t home_network_location_start(const char **error);
cJSON *home_network_location_json(void);
esp_err_t home_usb_start(void);
esp_err_t home_server_start(void);
void home_sources_task(void *);
bool home_hash(const void *, size_t, uint8_t[32]);
void home_render_locked(const home_config_t *, const home_data_t *, int, int64_t, uint8_t *);
bool home_localtime(const home_config_t *, time_t, struct tm *);
#endif
