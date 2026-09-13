#include "home_store.h"
#include "home_config.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_crc.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STORE_MAGIC 0x484f4d33U
#define STORE_MAX 8192U
typedef struct {
    uint32_t magic, version, sequence, size, crc;
} record_t;
typedef struct {
    double latitude, longitude;
    char feed_url[HOME_FEED_URL_BYTES];
    home_data_t data;
} cache_t;
static const char *TAG = "home_store";
static nvs_handle_t handle;
static uint32_t sequence[3];
static const char *keys[3][2] = {
    {"config0", "config1"}, {"data0", "data1"}, {"secret0", "secret1"}};
/* NVS only owns home_nvs. Never erase/format another partition or recover
 * corruption by erasing this one: a record that cannot be used is skipped and
 * the unit starts from defaults, leaving the old bytes until the next save.
 * Caller serializes NVS and state writes. */
static esp_err_t put(int kind, const void *data, size_t size)
{
    if (size > STORE_MAX || sequence[kind] == UINT32_MAX)
        return ESP_ERR_INVALID_SIZE;
    record_t *r = malloc(sizeof(*r) + size);
    if (!r)
        return ESP_ERR_NO_MEM;
    r->magic = STORE_MAGIC;
    r->version = 1;
    r->sequence = sequence[kind] + 1;
    r->size = size;
    memcpy(r + 1, data, size);
    r->crc = esp_crc32_le(0, (uint8_t *)(r + 1), size);
    esp_err_t e = nvs_set_blob(handle, keys[kind][r->sequence & 1U], r, sizeof(*r) + size);
    if (e == ESP_OK)
        e = nvs_commit(handle);
    if (e == ESP_OK)
        sequence[kind] = r->sequence;
    memset(r, 0, sizeof(*r) + size);
    free(r);
    return e;
}
static void *get_record(int kind, size_t *out_size, esp_err_t *error)
{
    record_t *best = NULL;
    bool damaged = false;
    *error = ESP_ERR_NVS_NOT_FOUND;
    for (int i = 0; i < 2; i++) {
        size_t size = 0;
        esp_err_t e = nvs_get_blob(handle, keys[kind][i], NULL, &size);
        if (e == ESP_ERR_NVS_NOT_FOUND)
            continue;
        if (e != ESP_OK || size < sizeof(record_t) || size > STORE_MAX + sizeof(record_t)) {
            damaged = true;
            continue;
        }
        record_t *r = malloc(size);
        if (!r) {
            free(best);
            *error = ESP_ERR_NO_MEM;
            return NULL;
        }
        if (nvs_get_blob(handle, keys[kind][i], r, &size) != ESP_OK || r->magic != STORE_MAGIC ||
            r->version != 1 || !r->sequence || r->size != size - sizeof(*r) ||
            r->crc != esp_crc32_le(0, (uint8_t *)(r + 1), r->size)) {
            free(r);
            damaged = true;
            continue;
        }
        if (!best || r->sequence > best->sequence) {
            free(best);
            best = r;
        } else
            free(r);
    }
    if (!best) {
        if (damaged)
            *error = ESP_ERR_INVALID_STATE;
        return NULL;
    }
    sequence[kind] = best->sequence;
    *out_size = best->size;
    void *data = malloc(best->size + 1);
    if (data) {
        memcpy(data, best + 1, best->size);
        ((char *)data)[best->size] = 0;
    }
    memset(best, 0, sizeof(*best) + best->size);
    free(best);
    *error = data ? ESP_OK : ESP_ERR_NO_MEM;
    return data;
}
esp_err_t home_store_init(home_config_t *c, home_data_t *d, home_secrets_t *s)
{
    esp_err_t e = nvs_flash_init_partition("home_nvs");
    if (e != ESP_OK)
        return e;
    e = nvs_open_from_partition("home_nvs", "home3", NVS_READWRITE, &handle);
    if (e != ESP_OK)
        return e;
    home_config_defaults(c);
    memset(d, 0, sizeof(*d));
    memset(s, 0, sizeof(*s));
    size_t size;
    void *p = get_record(0, &size, &e);
    if (e == ESP_ERR_NO_MEM)
        return e;
    if (e == ESP_ERR_INVALID_STATE)
        ESP_LOGW(TAG, "Saved settings damaged; starting with defaults");
    if (p) {
        char error[128];
        home_config_t loaded;
        if (home_config_decode(p, size, &loaded, NULL, false, error))
            *c = loaded;
        else
            ESP_LOGW(TAG, "Saved settings rejected (%s); starting with defaults", error);
        free(p);
    }
    p = get_record(1, &size, &e);
    if (e == ESP_ERR_NO_MEM)
        return e;
    if (p) {
        if (size == sizeof(cache_t)) {
            cache_t *cache = p;
            cache->feed_url[HOME_FEED_URL_BYTES - 1] = 0;
            if (cache->latitude == c->latitude && cache->longitude == c->longitude)
                d->weather = cache->data.weather;
            if (!strcmp(cache->feed_url, c->feed_url))
                d->feed = cache->data.feed;
        }
        free(p);
    }
    p = get_record(2, &size, &e);
    if (e == ESP_ERR_NO_MEM)
        return e;
    if (e == ESP_ERR_INVALID_STATE)
        ESP_LOGW(TAG, "Saved network and pairing data damaged; starting setup");
    if (p) {
        /* A different layout means another firmware wrote it: start setup
         * (new access point password, pairing) instead of halting. */
        if (size == sizeof(*s))
            memcpy(s, p, size);
        else
            ESP_LOGW(TAG, "Saved network and pairing data has another layout; starting setup");
        memset(p, 0, size);
        free(p);
    }
    /* Bounded persisted strings remain strings even after a version mismatch. */
    s->ssid[32] = 0;
    s->password[63] = 0;
    s->ap_password[16] = 0;
    s->ap_ssid[32] = 0;
    d->weather.meta.error[96] = 0;
    d->feed.meta.error[96] = 0;
    d->feed.title[256] = 0;
    d->feed.source[96] = 0;
    d->feed.url[512] = 0;
    return ESP_OK;
}
esp_err_t home_store_config(const home_config_t *c)
{
    cJSON *j = home_config_json(c, false);
    if (!j)
        return ESP_ERR_NO_MEM;
    char *s = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    if (!s)
        return ESP_ERR_NO_MEM;
    esp_err_t e = put(0, s, strlen(s));
    free(s);
    return e;
}
esp_err_t home_store_data(const home_data_t *d, const home_config_t *c)
{
    cache_t *p = calloc(1, sizeof(*p));
    if (!p)
        return ESP_ERR_NO_MEM;
    p->latitude = c->latitude;
    p->longitude = c->longitude;
    snprintf(p->feed_url, sizeof(p->feed_url), "%s", c->feed_url);
    p->data = *d;
    if (p->data.weather.meta.no_store)
        memset(&p->data.weather, 0, sizeof(p->data.weather));
    if (p->data.feed.meta.no_store)
        memset(&p->data.feed, 0, sizeof(p->data.feed));
    esp_err_t e = put(1, p, sizeof(*p));
    free(p);
    return e;
}
esp_err_t home_store_secrets(const home_secrets_t *s)
{
    return put(2, s, sizeof(*s));
}
