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
    int phase; /* 0ready,1preparing,2refreshing,3error */
    int displayed_screen, pending_screen;
    uint32_t generation, refresh_ms, render_ms;
    uint64_t request_id, manual_id;
    bool pending_manual;
    int64_t manual_until, last_switch, pair_until;
    uint8_t refresh_requested;
    unsigned api_active;
    char address[32], hostname[40], ssid[33], pair_code[7];
    uint8_t *frame;
} home_runtime_t;
extern home_runtime_t home_runtime;
void home_lock(void);
void home_unlock(void);
void home_begin_pairing(void);
esp_err_t home_network_start(void);
void home_network_apply(void);
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
