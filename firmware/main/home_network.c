#include "home_runtime.h"
#include "home_fetch.h"
#include "home_places.h"
#include "home_config.h"
#include "home_power.h"
#include "home_wake.h"
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

/* Beacons the station may sleep through between wake-ups (see esp_wifi_set_ps() below). */
#define HOME_LISTEN_INTERVAL 3
/* Breath mode (see control_task). Background work - a fetch, the first clock sync - may keep the
 * radio on this long without anything in progress; after that it waits, so a router that is gone
 * cannot hold the radio on for good. */
#define HOME_BACKGROUND_US INT64_C(90000000)
/* The wait doubles after each such session, up to an hour: a router that stays off overnight
 * must not cost what a normal night costs a second time. A session that gets its work done - the
 * clock set and nothing left due - resets it (since 0.6.2; before, the first address did). */
#define HOME_RETRY_US INT64_C(900000000)
#define HOME_RETRY_MAX_US INT64_C(3600000000)
/* After connecting the radio stays on a little: SNTP waits up to five seconds before its first
 * request, and the .local name is announced as soon as the address arrives. */
#define HOME_HOLD_US INT64_C(10000000)
/* A stop posts its events to the event loop; a start right behind it could receive them. */
#define HOME_OFF_MIN_US INT64_C(5000000)
/* While the radio is on, sources due within this many seconds are fetched in the same session. */
#define HOME_BATCH_S 300
/* The chip's own clock drifts while it sleeps. A connection re-syncs it at most once an hour; with
 * no screen that downloads anything (Note or Sky only) nothing else wakes the radio, so the clock
 * asks for a sync of its own after this long (0.6.2). */
#define HOME_CLOCK_RESYNC_S 21600

static esp_netif_t *station;
static int64_t next_connect;
static bool started;
/* Whether Wi-Fi is started. Breath mode stops it between fetches; control_task alone switches it,
 * under home_lock, and the event handler reads it to tell a deliberate stop from a lost network. */
static bool radio_on;
/* Seconds with the radio on: the whole day in Open mode, the fetches and the panel windows in
 * Breath. Logged per hour in the power log. */
static int64_t radio_us, radio_at;
int64_t home_network_radio_seconds(void)
{
    return radio_us / 1000000;
}
bool home_network_radio_on(void)
{
    return radio_on;
}
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
        if (home_runtime.wifi_pending || !radio_on || strcmp(actual_ssid, home_runtime.secrets.ssid)) {
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
        /* A new network being applied, or Breath switching the radio off: neither is an error. */
        if (home_runtime.wifi_pending || !radio_on) {
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
        ESP_LOGW(TAG, "Station offline (reason %d: %s); retry scheduled", event->reason, wifi_error);
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
        /* A stopped scan normally ends with its done event; if none comes, give up after 20 s
         * rather than hold the radio on for a scan that is over. */
        if (done || now - scan_started > INT64_C(20000000)) {
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
/* Set from the SNTP callback once a server has actually set the clock, and when (uptime seconds). */
static volatile bool sntp_synced;
static volatile uint32_t sntp_at_s;
static void on_sntp_sync(struct timeval *tv)
{
    (void)tv;
    sntp_at_s = (uint32_t)(esp_timer_get_time() / 1000000);
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
    /* Scan and join on channels 1-13, not only 1-11 of the world-safe default: European
     * routers pick 12 and 13 by themselves, and a station that never looks there reports
     * "network not found" for a network everyone else sees (15.09.2026). The policy stays
     * AUTO, so once associated the station follows the country its access point announces. */
    wifi_country_t country = {.cc = "01", .schan = 1, .nchan = 13, .policy = WIFI_COUNTRY_POLICY_AUTO};
    if ((e = esp_wifi_set_country(&country)) != ESP_OK)
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
    /* MAX_MODEM with a listen interval of three beacons, set explicitly rather than left to the
     * IDF default, which can change under us with the next IDF release. With MIN_MODEM the Wi-Fi
     * driver woke about seven times a second and held APB_FREQ_MAX about 50 ms each time: that
     * lock outranks light sleep and kept the chip awake about a third of the time, whatever our
     * own code did. With every third beacon it wakes about three times a second and the share
     * falls to under a fifth: the chip is free to sleep about 80 % of the time. The cost is latency
     * for frames the access point buffers, multicast mDNS included: measured, the panel still
     * answers in well under a second and home-xxxx.local still resolves. A longer interval would
     * save more but risks both, so it stays at three. */
    esp_err_t ps = esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    if (ps != ESP_OK)
        ESP_LOGW(TAG, "Wi-Fi power save not set: %s", esp_err_to_name(ps));
    started = true;
    radio_on = true;
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
    config.sta.listen_interval = HOME_LISTEN_INTERVAL; /* see the comment at esp_wifi_set_ps() */
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN; /* every channel, then the strongest match */
    config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
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
/* Breath mode turns the radio off whenever nothing needs it (home_radio_wanted) and on again for a
 * fetch, a press of OK or a reason that holds the chip awake anyway. Open mode never turns it off.
 * The stop happens here and nowhere else: under the lock the radio is marked off and the station
 * offline first, so the source worker cannot start a fetch on a connection about to disappear and
 * the event handler does not mistake the stop for a lost network. */
static void control_task(void *unused)
{
    (void)unused;
    bool was_online = false;
    int64_t hold_until = 0, retry_at = 0, retry_gap = HOME_RETRY_US, background_since = 0,
            off_at = 0;
    while (true) {
        /* Once a second. Nothing here is urgent: reconnection waits fifteen to thirty seconds
         * anyway, and the setup access point opens for five minutes. */
        vTaskDelay(pdMS_TO_TICKS(1000));
        int64_t mono = esp_timer_get_time();
        home_power_state_t pw;
        home_power_snapshot(&pw);
        home_lock();
        bool on = radio_on;
        if (on) {
            if (radio_at)
                radio_us += mono - radio_at;
            radio_at = mono;
        } else
            radio_at = 0;
        bool online = home_runtime.online, pending = home_runtime.wifi_pending;
        bool has_ssid = home_runtime.secrets.ssid[0] != 0;
        bool want_ap = mono < home_runtime.pair_until;
        int64_t reconnect_at = next_connect;
        home_radio_in_t in = {
            .breath = home_runtime.config.power_mode == HOME_POWER_BREATH,
            .reasons = pw.reasons,
            .now = mono,
            .awake_until = home_runtime.awake_until,
            /* Not wifi_pending: a new network is applied by this task, between two decisions,
             * and a network that keeps failing must not hold the radio on for good. */
            .busy = home_runtime.source_active || home_runtime.api_active || scan_requested ||
                    scan_active || scan_draining,
            .fetch_due = home_sources_due(&home_runtime.config, &home_runtime.data, time(NULL), 0),
            .clock_unset = !sntp_synced ||
                           (uint32_t)(mono / 1000000) - sntp_at_s >= HOME_CLOCK_RESYNC_S,
            .hold_until = hold_until,
            .retry_at = retry_at,
        };
        /* Background only: the radio is on for a fetch or the clock, nobody is waiting for it and
         * nothing is in progress. That may not last longer than HOME_BACKGROUND_US. */
        home_radio_in_t fg = in;
        fg.fetch_due = fg.clock_unset = false;
        fg.hold_until = 0;
        if (!on || home_radio_wanted(&fg))
            background_since = 0;
        else if (!background_since)
            background_since = mono;
        else if (mono - background_since > HOME_BACKGROUND_US) {
            retry_at = in.retry_at = mono + retry_gap;
            ESP_LOGW(TAG, "Radio on for %llds with nothing done; next try in %lld minutes",
                     (long long)(HOME_BACKGROUND_US / 1000000), (long long)(retry_gap / 60000000));
            retry_gap = retry_gap * 2 > HOME_RETRY_MAX_US ? HOME_RETRY_MAX_US : retry_gap * 2;
            background_since = 0;
        }
        bool want = home_radio_wanted(&in);
        if (on && !want) {
            radio_on = false;
            home_runtime.online = false;
            station_connecting = false;
        }
        if (!on && want && mono - off_at >= HOME_OFF_MIN_US) {
            radio_on = true;
            station_connecting = has_ssid && !pending;
            wifi_error[0] = 0;
            next_connect = pending ? mono : mono + INT64_C(30000000);
        }
        bool turn_on = !on && radio_on;
        home_unlock();
        if (on && !want) {
            esp_err_t e = esp_wifi_stop();
            off_at = mono;
            was_online = false;
            /* Off with the clock set and nothing due: the session did its work, so the pause
             * after a failed one starts over. Until 0.6.2 every connection reset it, and a
             * blocked time server kept the radio on for minutes an hour (0.6.2). */
            if (!in.clock_unset && !in.fetch_due) {
                retry_gap = HOME_RETRY_US;
                retry_at = 0; /* and an old pause no longer holds back the next fetch */
            }
            ESP_LOGI(TAG, "Radio off: %s", esp_err_to_name(e));
            continue;
        }
        if (!on && !turn_on)
            continue;
        if (want_ap != ap_enabled) {
            esp_err_t e = esp_wifi_set_mode(want_ap ? WIFI_MODE_APSTA : WIFI_MODE_STA);
            if (e == ESP_OK) {
                ap_enabled = want_ap;
                ESP_LOGI(TAG, "Setup access point %s", want_ap ? "opened" : "closed");
            }
        }
        if (turn_on) {
            esp_err_t e = esp_wifi_start();
            if (e != ESP_OK) { /* marked off again, so the next turn tries the start once more */
                home_lock();
                radio_on = false;
                station_connecting = false;
                home_unlock();
                off_at = mono;
                ESP_LOGW(TAG, "Radio start failed: %s", esp_err_to_name(e));
                continue;
            }
            esp_wifi_set_ps(WIFI_PS_MAX_MODEM); /* as at boot; see home_network_start() */
            if (has_ssid && !pending)
                e = esp_wifi_connect();
            ESP_LOGI(TAG, "Radio on: %s", esp_err_to_name(e));
            continue;
        }
        if (online && !was_online) {
            hold_until = mono + HOME_HOLD_US;
            /* The clock drifts while the chip sleeps and the radio is off, but the time servers
             * are still asked at most once an hour, as the privacy notes say. */
            if (!sntp_synced || (uint32_t)(mono / 1000000) - sntp_at_s >= 3600)
                esp_netif_sntp_start();
        }
        was_online = online;
        bool scanning = scan_step(mono, online);
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
        /* Someone asked for fresh data (side button, the round one, or the panel): make those
         * sources due now. Until 0.5.1 the request only set a flag that nothing ever read, so
         * "Refresh" waited for the ordinary schedule and looked broken. The fetch functions keep
         * their own rule against hammering a provider in next_fetch, so the request has to clear
         * it rather than walk around it. */
        if (home_runtime.refresh_requested & 1U)
            home_runtime.data.weather.meta.next_fetch = 0;
        if (home_runtime.refresh_requested & 2U)
            home_runtime.data.feed.meta.next_fetch = 0;
        if (home_runtime.refresh_requested & 4U)
            home_runtime.data.air.meta.next_fetch = 0;
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
        /* Online is checked again under the same lock that marks the worker busy: Breath may
         * have switched the radio off since the first look, and a stop never waits for us. */
        if (home_runtime.maintenance || !home_runtime.online) {
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
        /* Breath: the radio is on now, so sources due in the next few minutes come along instead
         * of waking it again. Not after a failure - that source keeps its own pause. The fetch
         * functions check next_fetch themselves, hence zero on this copy. */
        if (c->power_mode == HOME_POWER_BREATH) {
            home_source_meta_t *m[] = {&d->weather.meta, &d->feed.meta, &d->air.meta};
            for (size_t i = 0; i < sizeof m / sizeof m[0]; i++)
                if (!m[i]->error[0] && now + HOME_BATCH_S >= m[i]->next_fetch)
                    m[i]->next_fetch = 0;
        }
        /* Only screens that are switched on: a screen nobody shows must not send the place or
         * the reader's address anywhere (air since 0.5.0, weather and headline since 0.6.2). */
        unsigned wanted = home_sources_wanted(c);
        bool changed = false;
        if ((wanted & 1U) && now >= d->weather.meta.next_fetch) {
            home_fetch_weather(c, &d->weather, now);
            home_lock();
            if (home_runtime.config.latitude == c->latitude &&
                home_runtime.config.longitude == c->longitude) {
                home_runtime.data.weather = d->weather;
                home_runtime.counters.fetches++;
                home_runtime.refresh_requested &= ~1U;
                home_runtime.dirty = true;
                home_runtime.request_id++;
                changed = true;
            }
            home_unlock();
        }
        if ((wanted & 2U) && now >= d->feed.meta.next_fetch) {
            home_fetch_feed(c, &d->feed, now);
            home_lock();
            if (!strcmp(home_runtime.config.feed_url, c->feed_url)) {
                home_runtime.data.feed = d->feed;
                home_runtime.counters.fetches++;
                home_runtime.refresh_requested &= ~2U;
                home_runtime.dirty = true;
                home_runtime.request_id++;
                changed = true;
            }
            home_unlock();
        }
        /* Same worker, so never two connections at once. */
        if ((wanted & 4U) && now >= d->air.meta.next_fetch) {
            home_fetch_air(c, &d->air, now);
            home_lock();
            if (home_runtime.config.latitude == c->latitude &&
                home_runtime.config.longitude == c->longitude &&
                home_runtime.config.enabled[HOME_AIR]) {
                home_runtime.data.air = d->air;
                home_runtime.counters.fetches++;
                home_runtime.refresh_requested &= ~4U;
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
            ESP_LOGI(TAG, "Source cycle complete; weather=%d feed=%d air=%d persistence=%s",
                     d->weather.meta.state, d->feed.meta.state, d->air.meta.state,
                     esp_err_to_name(e));
        }
        home_lock();
        home_runtime.source_active = false;
        home_unlock();
    }
}
