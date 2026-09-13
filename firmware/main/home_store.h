#ifndef HOME_STORE_H
#define HOME_STORE_H
#include "home_types.h"
#include "esp_err.h"
typedef struct {
    char ssid[33], password[64], ap_password[17], ap_ssid[33];
    uint8_t token_hash[4][32];
    uint8_t token_used[4];
} home_secrets_t;
esp_err_t home_store_init(home_config_t *, home_data_t *, home_secrets_t *);
esp_err_t home_store_config(const home_config_t *);
esp_err_t home_store_data(const home_data_t *, const home_config_t *);
esp_err_t home_store_secrets(const home_secrets_t *);
#endif
