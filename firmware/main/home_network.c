#include "home_runtime.h"
#include "home_fetch.h"
#include "home_places.h"
#include "home_config.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "lwip/ip4_addr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static esp_netif_t *station;
static int64_t next_connect;
static bool started;
static bool ap_enabled = true;
static const char *TAG = "home_net";
enum { SCAN_IDLE, SCAN_RUNNING, SCAN_READY, SCAN_ERROR };
static int scan_state;
static bool scan_requested, scan_done, scan_active, scan_draining;
static int64_t scan_started, scan_last;
static uint32_t scan_result;
static char wifi_error[40];
static bool station_connecting;
static uint32_t wifi_revision;
static home_location_t location_state;
typedef struct {
    char ssid[33];
    int rssi;
    bool secure, supported;
} network_t;
static network_t networks[20];
static unsigned network_count;
static void control_task(void *unused);
static void events(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        wifi_event_sta_scan_done_t *event = data;
        home_lock();
        scan_result = event->status;
        scan_done = true;
        home_unlock();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        /* Match the SDK Wi-Fi example: HTTPD already listens dual-stack.
         * A real link-local AAAA avoids waiting for an absent IPv6 answer. */
        esp_err_t e = esp_netif_create_ip6_linklocal(station);
        if (e != ESP_OK)
            ESP_LOGW(TAG, "Local IPv6 unavailable: %s", esp_err_to_name(e));
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        wifi_ap_record_t associated;
        if (esp_wifi_sta_get_ap_info(&associated) != ESP_OK)
            return;
        char actual_ssid[33];
        memcpy(actual_ssid, associated.ssid, 32);
        actual_ssid[32] = 0;
        home_lock();
        if (home_runtime.wifi_pending || strcmp(actual_ssid, home_runtime.secrets.ssid)) {
            home_unlock();
            return;
        }
        home_runtime.online = true;
        station_connecting = false;
        wifi_error[0] = 0;
        snprintf(home_runtime.address, sizeof(home_runtime.address), IPSTR,
                 IP2STR(&event->ip_info.ip));
        home_runtime.dirty = true;
        home_runtime.request_id++;
        home_unlock();
        ESP_LOGI(TAG, "Station acquired an IP address");
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = data;
        home_lock();
        home_runtime.online = false;
        if (home_runtime.wifi_pending) {
            home_unlock();
            return;
        }
        if (event->reason == WIFI_REASON_ASSOC_LEAVE && station_connecting) {
            home_unlock();
            return; /* intentional change of configured network */
        }
        station_connecting = false;
        if (event->reason == WIFI_REASON_AUTH_FAIL ||
            event->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
            event->reason == WIFI_REASON_HANDSHAKE_TIMEOUT)
            strcpy(wifi_error, "authentication_failed");
        else if (event->reason == WIFI_REASON_NO_AP_FOUND)
            strcpy(wifi_error, "network_not_found");
        else
            strcpy(wifi_error, "connection_lost");
        next_connect = esp_timer_get_time() + INT64_C(15000000);
        home_unlock();
        ESP_LOGW(TAG, "Station offline; retry scheduled");
    }
}
esp_err_t home_network_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password || !ssid[0] || !home_utf8(ssid, 32, false) ||
        !home_utf8(password, 63, false) || strlen(password) < 8)
        return ESP_ERR_INVALID_ARG;
    home_lock();
    if (home_runtime.maintenance) {
        home_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    home_secrets_t secrets = home_runtime.secrets;
    strcpy(secrets.ssid, ssid);
    strcpy(secrets.password, password);
    esp_err_t e = home_store_secrets(&secrets);
    if (e == ESP_OK) {
        home_runtime.secrets = secrets;
        home_runtime.wifi_pending = true;
        home_runtime.online = false; /* old association is not the requested network */
        station_connecting = true;
        wifi_revision++;
        next_connect = 0;
        wifi_error[0] = 0;
    }
    home_unlock();
    memset(&secrets, 0, sizeof(secrets));
    return e;
}
esp_err_t home_network_scan_start(void)
{
    home_lock();
    int64_t now = esp_timer_get_time();
    if (home_runtime.maintenance || scan_draining ||
        (scan_last && now - scan_last < INT64_C(10000000))) {
        home_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    if (scan_state != SCAN_RUNNING) {
        scan_requested = true;
        scan_state = SCAN_RUNNING;
        scan_last = now;
    }
    home_unlock();
    return ESP_OK;
}
esp_err_t home_network_location_start(const char **error)
{
    home_lock();
    bool ok = home_location_enqueue(&location_state, esp_timer_get_time(), home_runtime.online,
                                    home_runtime.time_valid, home_runtime.maintenance, error);
    home_unlock();
    return ok ? ESP_OK : ESP_ERR_INVALID_STATE;
}
cJSON *home_network_location_json(void)
{
    home_lock();
    home_location_expire(&location_state, esp_timer_get_time());
    cJSON *json = home_location_json(&location_state);
    home_unlock();
    return json;
}
cJSON *home_network_scan_json(void)
{
    const char *names[] = {"idle", "scanning", "ready", "error"};
    cJSON *j = cJSON_CreateObject();
    home_lock();
    bool ok = j && cJSON_AddStringToObject(j, "state", names[scan_state]);
    cJSON *list = cJSON_AddArrayToObject(j, "networks");
    ok = ok && list;
    for (unsigned i = 0; ok && i < network_count; ++i) {
        cJSON *n = cJSON_CreateObject();
        ok = n && cJSON_AddStringToObject(n, "ssid", networks[i].ssid) &&
             cJSON_AddNumberToObject(n, "rssi", networks[i].rssi) &&
             cJSON_AddBoolToObject(n, "secure", networks[i].secure) &&
             cJSON_AddBoolToObject(n, "supported", networks[i].supported) &&
             cJSON_AddBoolToObject(n, "connected",
                                   home_runtime.online &&
                                       !strcmp(networks[i].ssid, home_runtime.secrets.ssid));
        if (ok)
            ok = cJSON_AddItemToArray(list, n);
        if (!ok)
            cJSON_Delete(n);
    }
    if (scan_state == SCAN_ERROR)
        ok = ok && cJSON_AddStringToObject(j, "error", "scan_unavailable");
    home_unlock();
    if (!ok) {
        cJSON_Delete(j);
        return NULL;
    }
    return j;
}
cJSON *home_network_status_json(void)
{
    cJSON *j = cJSON_CreateObject();
    home_lock();
    const char *state = "unconfigured";
    if (home_runtime.online)
        state = "connected";
    else if (home_runtime.secrets.ssid[0])
        state = wifi_error[0] && !station_connecting ? "error" : "connecting";
    bool ok = j && cJSON_AddStringToObject(j, "ssid", home_runtime.secrets.ssid) &&
              cJSON_AddStringToObject(j, "state", state);
    ok = ok && cJSON_AddStringToObject(j, "error", wifi_error);
    home_unlock();
    if (!ok) {
        cJSON_Delete(j);
        return NULL;
    }
    return j;
}
/* A single control task owns scan/start/read/free and connect operations. */
static bool scan_step(int64_t now, bool online)
{
    home_lock();
    bool requested = scan_requested, done = scan_done;
    bool draining = scan_draining;
    uint32_t result = scan_result;
    scan_requested = scan_done = false;
    home_unlock();
    if (draining) {
        if (done) {
            esp_wifi_clear_ap_list();
            home_lock();
            scan_draining = false;
            home_unlock();
        }
        return false; /* stopped scan must not suspend station retries/AP expiry */
    }
    if (requested && !scan_active) {
        if (done)
            esp_wifi_clear_ap_list();
        done = false; /* never attribute an older completion to this new scan */
        if (!online)
            esp_wifi_disconnect();
        wifi_scan_config_t cfg = {.show_hidden = false,
                                  .scan_type = WIFI_SCAN_TYPE_ACTIVE,
                                  .scan_time.active = {.min = 0, .max = 120}};
        esp_err_t e = esp_wifi_scan_start(&cfg, false);
        scan_active = e == ESP_OK;
        scan_started = now;
        if (!scan_active) {
            esp_wifi_clear_ap_list();
            home_lock();
            scan_state = SCAN_ERROR;
            home_unlock();
        }
    }
    if (scan_active && (done || now - scan_started > INT64_C(10000000))) {
        if (!done) {
            home_lock();
            scan_draining = true;
            home_unlock();
            esp_wifi_scan_stop();
        }
        wifi_ap_record_t *records = calloc(40, sizeof(*records));
        uint16_t count = 40;
        esp_err_t e =
            records && done && !result ? esp_wifi_scan_get_ap_records(&count, records) : ESP_FAIL;
        if (e != ESP_OK)
            esp_wifi_clear_ap_list();
        home_lock();
        network_count = 0;
        if (e == ESP_OK)
            for (unsigned i = 0; i < count; ++i) {
                char ssid[33];
                memcpy(ssid, records[i].ssid, 32);
                ssid[32] = 0;
                if (!ssid[0] || !home_utf8(ssid, 32, false) || !strcmp(ssid, home_runtime.ssid))
                    continue;
                bool supported = records[i].authmode == WIFI_AUTH_WPA2_PSK ||
                                 records[i].authmode == WIFI_AUTH_WPA_WPA2_PSK ||
                                 records[i].authmode == WIFI_AUTH_WPA3_PSK ||
                                 records[i].authmode == WIFI_AUTH_WPA2_WPA3_PSK;
                network_t *pick = NULL;
                for (unsigned n = 0; n < network_count; ++n)
                    if (!strcmp(ssid, networks[n].ssid))
                        pick = &networks[n];
                if (pick && (pick->supported || !supported))
                    continue;
                if (!pick && network_count >= 20)
                    continue;
                network_t *n = pick ? pick : &networks[network_count++];
                strcpy(n->ssid, ssid);
                n->rssi = records[i].rssi;
                n->secure = records[i].authmode != WIFI_AUTH_OPEN;
                n->supported = supported;
            }
        for (unsigned i = 1; i < network_count; ++i) {
            network_t value = networks[i];
            unsigned at = i;
            while (at && networks[at - 1].rssi < value.rssi) {
                networks[at] = networks[at - 1];
                --at;
            }
            networks[at] = value;
        }
        scan_state = e == ESP_OK ? SCAN_READY : SCAN_ERROR;
        home_unlock();
        free(records);
        scan_active = false;
    } else if (done && !scan_active)
        esp_wifi_clear_ap_list();
    return scan_active;
}
/* Set from the SNTP callback once a server has actually set the clock. */
static volatile bool sntp_synced;
static void on_sntp_sync(struct timeval *tv)
{
    (void)tv;
    sntp_synced = true;
}
esp_err_t home_network_start(void)
{
    esp_err_t e = esp_netif_init();
    if (e != ESP_OK)
        return e;
    e = esp_event_loop_create_default();
    if (e != ESP_OK)
        return e;
    if (!esp_netif_create_default_wifi_ap())
        return ESP_ERR_NO_MEM;
    station = esp_netif_create_default_wifi_sta();
    if (!station)
        return ESP_ERR_NO_MEM;
    esp_netif_set_hostname(station, "emini-home");
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    init.nvs_enable = 0;
    if ((e = esp_wifi_init(&init)) != ESP_OK)
        return e;
    if ((e = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK)
        return e;
    if ((e = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, events, NULL)) != ESP_OK)
        return e;
    if ((e = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, events, NULL)) != ESP_OK)
        return e;
    if ((e = esp_wifi_set_mode(WIFI_MODE_APSTA)) != ESP_OK)
        return e;
    wifi_config_t ap = {0};
    ap.ap.ssid_len = strlen(home_runtime.ssid);
    memcpy(ap.ap.ssid, home_runtime.ssid, ap.ap.ssid_len);
    ap.ap.channel = 1;
    ap.ap.max_connection = 3;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    snprintf((char *)ap.ap.password, sizeof(ap.ap.password), "%s",
             home_runtime.secrets.ap_password);
    ap.ap.pmf_cfg.required = false;
    if ((e = esp_wifi_set_config(WIFI_IF_AP, &ap)) != ESP_OK)
        return e;
    bool paired = false;
    for (unsigned i = 0; i < 4; ++i)
        paired |= home_runtime.secrets.token_used[i] != 0;
    ap_enabled = !home_runtime.secrets.ssid[0] || !paired;
    /* Choose the final boot mode before starting association. Switching APSTA
     * to STA from the control task used to race the first connection attempt. */
    if (!ap_enabled && (e = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK)
        return e;
    if ((e = esp_wifi_start()) != ESP_OK)
        return e;
    started = true;
    esp_sntp_config_t ntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    ntp.start = true;
    ntp.wait_for_sync = false;
    ntp.sync_cb = on_sntp_sync;
    if ((e = esp_netif_sntp_init(&ntp)) != ESP_OK)
        return e;
    home_network_apply();
    if (xTaskCreate(control_task, "home_net_control", 6144, NULL, 5, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    return ESP_OK;
}
void home_network_apply(void)
{
    if (!started)
        return;
    wifi_config_t config = {0};
    home_lock();
    memcpy(config.sta.ssid, home_runtime.secrets.ssid, strlen(home_runtime.secrets.ssid));
    snprintf((char *)config.sta.password, sizeof(config.sta.password), "%s",
             home_runtime.secrets.password);
    bool has_ssid = home_runtime.secrets.ssid[0] != 0;
    uint32_t revision = wifi_revision;
    home_runtime.wifi_pending = has_ssid;
    station_connecting = has_ssid;
    wifi_error[0] = 0;
    home_unlock();
    if (!has_ssid)
        return;
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    config.sta.pmf_cfg.capable = true;
    esp_wifi_disconnect();
    esp_err_t e = esp_wifi_set_config(WIFI_IF_STA, &config);
    memset(config.sta.password, 0, sizeof(config.sta.password));
    home_lock();
    bool current = revision == wifi_revision;
    if (current && e == ESP_OK)
        home_runtime.wifi_pending = false;
    home_unlock();
    if (e == ESP_OK && current)
        e = esp_wifi_connect();
    home_lock();
    if (revision == wifi_revision) {
        next_connect = esp_timer_get_time() + INT64_C(15000000);
        if (e != ESP_OK) {
            home_runtime.wifi_pending = true;
            station_connecting = false;
            strcpy(wifi_error, "configuration_failed");
        }
    }
    home_unlock();
    if (e != ESP_OK)
        ESP_LOGW(TAG, "Station configuration/connect failed: %s", esp_err_to_name(e));
}
static void control_task(void *unused)
{
    (void)unused;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(250));
        int64_t mono = esp_timer_get_time();
        home_lock();
        bool online = home_runtime.online, pending = home_runtime.wifi_pending;
        bool has_ssid = home_runtime.secrets.ssid[0] != 0;
        bool want_ap = mono < home_runtime.pair_until;
        int64_t reconnect_at = next_connect;
        home_unlock();
        bool scanning = scan_step(mono, online);
        if (want_ap != ap_enabled) {
            esp_err_t e = esp_wifi_set_mode(want_ap ? WIFI_MODE_APSTA : WIFI_MODE_STA);
            if (e == ESP_OK) {
                ap_enabled = want_ap;
                ESP_LOGI(TAG, "Setup access point %s", want_ap ? "opened" : "closed");
            }
        }
        if (pending && !scanning && mono >= reconnect_at) {
            home_network_apply();
        } else if (!pending && !scanning && !online && has_ssid && mono >= reconnect_at) {
            esp_err_t e = esp_wifi_connect();
            home_lock();
            station_connecting = e == ESP_OK;
            next_connect = mono + INT64_C(30000000);
            home_unlock();
        }
    }
}
void home_sources_task(void *unused)
{
    (void)unused;
    home_config_t *c = malloc(sizeof(*c));
    home_data_t *d = malloc(sizeof(*d));
    if (!c || !d) {
        ESP_LOGE(TAG, "Source worker allocation failed");
        free(c);
        free(d);
        vTaskDelete(NULL);
        return;
    }
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        time_t now = time(NULL);
        home_lock();
        bool online = home_runtime.online;
        *c = home_runtime.config;
        *d = home_runtime.data;
        int32_t offset;
        bool dst;
        /* The clock counts only once SNTP has set it. A time carried over a reset by the
         * RTC can be minutes off, and data stamped with it would sit "in the future"
         * after the sync, showing "Age unknown" on every screen (seen 15.09.2026). */
        bool valid_clock =
            sntp_synced && now >= 1704067200 && home_tz_offset_at(c->timezone, now, &offset, &dst);
        bool clock_changed = home_runtime.time_valid != valid_clock;
        home_runtime.time_valid = valid_clock;
        if (clock_changed) {
            home_runtime.dirty = true;
            home_runtime.request_id++;
        }
        home_unlock();
        if (!online || !valid_clock)
            continue;
        home_lock();
        if (home_runtime.maintenance) {
            home_unlock();
            continue;
        }
        home_runtime.source_active = true;
        uint32_t area_generation = 0, area_network = wifi_revision;
        bool area_requested =
            home_location_take(&location_state, esp_timer_get_time(), &area_generation);
        home_unlock();
        if (area_requested) {
            char *json = NULL;
            size_t size = 0;
            home_area_t area = {0};
            esp_err_t e = home_fetch_location(&json, &size);
            bool parsed = e == ESP_OK && home_location_parse(json, size, &area);
            if (json) {
                memset(json, 0, size);
                free(json);
            }
            home_lock();
            bool same_network = area_network == wifi_revision && home_runtime.online;
            home_location_finish(&location_state, area_generation,
                                 parsed && same_network ? &area : NULL,
                                 !same_network ? "location_connection_changed"
                                 : e == ESP_OK ? "location_invalid_response"
                                               : "location_request_failed");
            home_runtime.source_active = false;
            home_unlock();
            continue; /* same worker, never simultaneous TLS or config/NVS write */
        }
        bool changed = false;
        if (c->location_ready && now >= d->weather.meta.next_fetch) {
            home_fetch_weather(c, &d->weather, now);
            home_lock();
            if (home_runtime.config.latitude == c->latitude &&
                home_runtime.config.longitude == c->longitude) {
                home_runtime.data.weather = d->weather;
                home_runtime.refresh_requested &= ~1U;
                home_runtime.dirty = true;
                home_runtime.request_id++;
                changed = true;
            }
            home_unlock();
        }
        if (c->feed_url[0] && now >= d->feed.meta.next_fetch) {
            home_fetch_feed(c, &d->feed, now);
            home_lock();
            if (!strcmp(home_runtime.config.feed_url, c->feed_url)) {
                home_runtime.data.feed = d->feed;
                home_runtime.refresh_requested &= ~2U;
                home_runtime.dirty = true;
                home_runtime.request_id++;
                changed = true;
            }
            home_unlock();
        }
        if (changed) {
            home_lock();
            esp_err_t e = home_store_data(&home_runtime.data, &home_runtime.config);
            home_unlock();
            ESP_LOGI(TAG, "Source cycle complete; weather=%d feed=%d persistence=%s",
                     d->weather.meta.state, d->feed.meta.state, esp_err_to_name(e));
        }
        home_lock();
        home_runtime.source_active = false;
        home_unlock();
    }
}
