#include "home_runtime.h"
#include "home_power.h"
#include "home_panel.h"
#ifdef HOME_PM_PROFILE
#include "esp_pm.h"
#endif
#include "home_config.h"
#include "home_places.h"
#include "home_wake.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "esp_app_desc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define EMBED(n, s)                                                                                \
    extern const uint8_t n##_start[] asm("_binary_" s "_start");                                   \
    extern const uint8_t n##_end[] asm("_binary_" s "_end")
EMBED(index_html, "index_html");
EMBED(device_css, "device_css");
EMBED(device_js, "device_js");
EMBED(core_js, "core_js");
static httpd_handle_t server;
static unsigned pair_attempts;
static int64_t attempt_window;
static const char *phases[] = {"ready", "preparing", "refreshing", "error"};
static const char *states[] = {"empty", "ready", "stale", "error"};

static esp_err_t json_text(httpd_req_t *r, const char *s)
{
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_send(r, s, HTTPD_RESP_USE_STRLEN);
}
static bool json_child(cJSON *parent, const char *key, cJSON *child)
{
    if (child && cJSON_AddItemToObject(parent, key, child))
        return true;
    cJSON_Delete(child);
    return false;
}
static esp_err_t json_send(httpd_req_t *r, cJSON *j)
{
    if (!j)
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    char *s = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    if (!s)
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    esp_err_t e = json_text(r, s);
    free(s);
    return e;
}
static esp_err_t error(httpd_req_t *r, const char *status, const char *message)
{
    httpd_resp_set_status(r, status);
    cJSON *j = cJSON_CreateObject();
    if (!cJSON_AddStringToObject(j, "error", message)) {
        cJSON_Delete(j);
        j = NULL;
    }
    return json_send(r, j);
}
static esp_err_t accepted(httpd_req_t *r)
{
    httpd_resp_set_status(r, "202 Accepted");
    cJSON *j = cJSON_CreateObject();
    if (!cJSON_AddBoolToObject(j, "accepted", true)) {
        cJSON_Delete(j);
        j = NULL;
    }
    return json_send(r, j);
}
static bool header(httpd_req_t *r, const char *k, char *out, size_t n)
{
    size_t len = httpd_req_get_hdr_value_len(r, k);
    if (!len || len >= n)
        return false;
    return httpd_req_get_hdr_value_str(r, k, out, n) == ESP_OK;
}
static bool origin_ok(httpd_req_t *r)
{
    char host[96], origin[128], expected[128], ip[32], hostname[40];
    if (!header(r, "Host", host, sizeof(host)))
        return false;
    char clean[96];
    snprintf(clean, sizeof(clean), "%s", host);
    char *colon = strchr(clean, ':');
    if (colon) {
        if (strcmp(colon, ":80"))
            return false;
        *colon = 0;
    }
    home_lock();
    snprintf(ip, sizeof(ip), "%s", home_runtime.address);
    snprintf(hostname, sizeof(hostname), "%s", home_runtime.hostname);
    home_unlock();
    if (strcmp(clean, "192.168.4.1") && strcmp(clean, ip) &&
        (!hostname[0] || strcmp(clean, hostname)))
        return false;
    if (httpd_req_get_hdr_value_len(r, "Origin")) {
        if (!header(r, "Origin", origin, sizeof(origin)))
            return false;
        snprintf(expected, sizeof(expected), "http://%s", host);
        if (strcmp(origin, expected))
            return false;
    }
    return true;
}
static int auth(httpd_req_t *r)
{
    char value[80];
    if (!header(r, "Authorization", value, sizeof(value)) || strncmp(value, "Bearer ", 7) ||
        strlen(value + 7) != 64)
        return -1;
    for (int i = 7; i < 71; i++)
        if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f')))
            return -1;
    uint8_t hash[32];
    if (!home_hash(value + 7, 64, hash))
        return -1;
    int match = -1;
    home_lock();
    for (int i = 0; i < 4; i++) {
        volatile unsigned diff = 0;
        for (int n = 0; n < 32; n++)
            diff |= hash[n] ^ home_runtime.secrets.token_hash[i][n];
        if (!diff && home_runtime.secrets.token_used[i])
            match = i;
    }
    home_unlock();
    memset(value, 0, sizeof(value));
    return match;
}
static char *body(httpd_req_t *r)
{
    char type[80];
    if (!header(r, "Content-Type", type, sizeof(type)) || strncmp(type, "application/json", 16) ||
        (type[16] && type[16] != ';') || r->content_len <= 0 || r->content_len > 8192)
        return NULL;
    char *b = malloc(r->content_len + 1);
    if (!b)
        return NULL;
    size_t pos = 0;
    int64_t start = esp_timer_get_time();
    while (pos < r->content_len) {
        if (esp_timer_get_time() - start > 5000000) {
            free(b);
            return NULL;
        }
        int n = httpd_req_recv(r, b + pos, r->content_len - pos);
        if (n <= 0) {
            free(b);
            return NULL;
        }
        pos += n;
    }
    b[pos] = 0;
    if (strlen(b) != pos || strstr(b, "\\u0000")) {
        free(b);
        return NULL;
    }
    return b;
}
static cJSON *small_json(const char *b)
{
    int depth = 0;
    bool quoted = false, escape = false;
    for (const unsigned char *p = (const unsigned char *)b; *p; p++) {
        if (quoted) {
            if (*p < 32)
                return NULL;
            if (escape)
                escape = false;
            else if (*p == '\\')
                escape = true;
            else if (*p == '"')
                quoted = false;
        } else if (*p == '"')
            quoted = true;
        else if (*p == '{' || *p == '[') {
            if (++depth > 12)
                return NULL;
        } else if (*p == '}' || *p == ']') {
            if (--depth < 0)
                return NULL;
        }
    }
    if (quoted || depth)
        return NULL;
    return cJSON_ParseWithLengthOpts(b, strlen(b) + 1, NULL, true);
}
static bool only(cJSON *j, const char *a, const char *b)
{
    if (!cJSON_IsObject(j))
        return false;
    for (cJSON *v = j->child; v; v = v->next) {
        if (strcmp(v->string, a) && (!b || strcmp(v->string, b)))
            return false;
        for (cJSON *w = v->next; w; w = w->next)
            if (!strcmp(w->string, v->string))
                return false;
    }
    return true;
}
static cJSON *meta_json(const home_source_meta_t *m)
{
    cJSON *j = cJSON_CreateObject();
    bool ok =
        j &&
        cJSON_AddStringToObject(
            j, "state",
            states[m->state >= HOME_EMPTY && m->state <= HOME_ERROR ? m->state : HOME_ERROR]) &&
        cJSON_AddBoolToObject(j, "valid", m->valid) &&
        cJSON_AddNumberToObject(j, "issued_at", m->issued_at) &&
        cJSON_AddNumberToObject(j, "fetched_at", m->fetched_at) &&
        cJSON_AddNumberToObject(j, "checked_at", m->checked_at) &&
        cJSON_AddNumberToObject(j, "expires_at", m->expires_at) &&
        cJSON_AddNumberToObject(j, "next_fetch", m->next_fetch) &&
        cJSON_AddStringToObject(j, "error", m->error);
    if (!ok) {
        cJSON_Delete(j);
        return NULL;
    }
    return j;
}
static cJSON *feed_json(const home_feed_t *f)
{
    cJSON *j = meta_json(&f->meta);
    bool ok = j && cJSON_AddStringToObject(j, "title", f->title) &&
              cJSON_AddStringToObject(j, "source", f->source) &&
              cJSON_AddStringToObject(j, "url", f->url) &&
              cJSON_AddNumberToObject(j, "published_at", f->published_at);
    if (!ok) {
        cJSON_Delete(j);
        return NULL;
    }
    return j;
}
/* The power log: a week of hours, oldest first. Behind the token, because it tells the rhythm of
 * the household (when the device sat on a cable, when someone touched it). */
static cJSON *power_log_json(void)
{
    cJSON *j = cJSON_CreateObject();
    cJSON *hours = j ? cJSON_AddArrayToObject(j, "hours") : NULL;
    if (!hours) {
        cJSON_Delete(j);
        return NULL;
    }
    home_lock();
    home_power_log_t log = home_runtime.power_log;
    home_unlock();
    int count = log.written < HOME_POWER_HOURS ? log.written : HOME_POWER_HOURS;
    int start = log.written < HOME_POWER_HOURS ? 0 : log.head;
    bool ok = cJSON_AddNumberToObject(j, "hours_kept", count) != NULL;
    for (int i = 0; ok && i < count; i++) {
        const home_power_hour_t *h = &log.hour[(start + i) % HOME_POWER_HOURS];
        if (!h->at)
            continue;
        cJSON *e = cJSON_CreateObject();
        ok = e && cJSON_AddNumberToObject(e, "at", h->at) &&
             (h->millivolts ? cJSON_AddNumberToObject(e, "millivolts", h->millivolts)
                            : cJSON_AddNullToObject(e, "millivolts")) &&
             cJSON_AddNumberToObject(e, "flags", h->flags) &&
             cJSON_AddNumberToObject(e, "pictures", h->pictures) &&
             cJSON_AddNumberToObject(e, "fetches", h->fetches) &&
             cJSON_AddNumberToObject(e, "held_s", h->held_s) &&
             cJSON_AddNumberToObject(e, "panel_s", h->panel_s) &&
             cJSON_AddNumberToObject(e, "radio_s", h->radio_s);
        if (ok)
            cJSON_AddItemToArray(hours, e);
        else
            cJSON_Delete(e);
    }
    if (!ok) {
        cJSON_Delete(j);
        return NULL;
    }
    return j;
}
static esp_err_t status(httpd_req_t *r, int token)
{
    cJSON *j = cJSON_CreateObject();
    home_lock();
    home_runtime_t *h = &home_runtime;
    int count = 0;
    for (int i = 0; i < 4; i++)
        count += h->secrets.token_used[i] ? 1 : 0;
    bool ok = j && cJSON_AddBoolToObject(j, "paired", count > 0) &&
              cJSON_AddBoolToObject(j, "online", h->online) &&
              cJSON_AddBoolToObject(j, "setup", h->setup) &&
              cJSON_AddStringToObject(j, "name", h->config.name) &&
              cJSON_AddStringToObject(j, "version", esp_app_get_description()->version) &&
              cJSON_AddStringToObject(j, "address", h->address) &&
              cJSON_AddStringToObject(j, "hostname", h->hostname) &&
              cJSON_AddStringToObject(j, "state", phases[h->phase]) &&
              cJSON_AddNumberToObject(j, "generation", h->generation);
    ok = ok &&
         (h->displayed_screen >= 0 ? cJSON_AddStringToObject(j, "displayed_screen",
                                                             home_screen_name(h->displayed_screen))
                                   : cJSON_AddNullToObject(j, "displayed_screen"));
    ok = ok && cJSON_AddBoolToObject(j, "pending", h->dirty || h->phase == 1 || h->phase == 2) &&
         cJSON_AddNumberToObject(j, "refresh_ms", h->refresh_ms) &&
         cJSON_AddNumberToObject(j, "render_ms", h->render_ms) &&
         cJSON_AddBoolToObject(j, "time_valid", h->time_valid) &&
         cJSON_AddNumberToObject(j, "pairing_seconds",
                                 h->pair_until > esp_timer_get_time()
                                     ? (h->pair_until - esp_timer_get_time()) / 1000000
                                     : 0) &&
         /* The last gesture the device made sense of: key 1 up, 2 down, 4 the round one,
          * how long it was held, and what it did. Lets a reader check a press without a cable. */
         cJSON_AddNumberToObject(j, "last_key", h->key_last) &&
         cJSON_AddNumberToObject(j, "last_key_ms", h->key_last_ms) &&
         cJSON_AddStringToObject(j, "last_key_action",
                                 h->key_last_what ? h->key_last_what : "") &&
         /* Seconds since the weather source was last asked: tells a gesture apart from a
          * screen that simply had nothing new to draw. */
         cJSON_AddNumberToObject(j, "weather_checked_s",
                                 h->data.weather.meta.checked_at > 0 && time(NULL) > 0
                                     ? (double)(time(NULL) - h->data.weather.meta.checked_at)
                                     : -1);
    if (token >= 0 && ok) {
        const home_battery_t *b = &h->battery;
        cJSON *battery = cJSON_AddObjectToObject(j, "battery");
        ok = battery && cJSON_AddBoolToObject(battery, "valid", b->valid) &&
             (b->valid ? cJSON_AddNumberToObject(battery, "millivolts", b->millivolts)
                       : cJSON_AddNullToObject(battery, "millivolts")) &&
             (b->valid && b->charge_valid && b->percent_estimate >= 0
                  ? cJSON_AddNumberToObject(battery, "percent_estimate", b->percent_estimate)
                  : cJSON_AddNullToObject(battery, "percent_estimate")) &&
             (b->charge_valid ? cJSON_AddBoolToObject(battery, "charging", b->charging)
                              : cJSON_AddNullToObject(battery, "charging")) &&
             (b->charge_valid ? cJSON_AddBoolToObject(battery, "full", b->full)
                              : cJSON_AddNullToObject(battery, "full")) &&
             cJSON_AddNumberToObject(battery, "measured_at", b->measured_at);
        cJSON *metrics = cJSON_AddObjectToObject(j, "metrics");
        ok = ok && metrics &&
             cJSON_AddNumberToObject(metrics, "free_internal",
                                     heap_caps_get_free_size(MALLOC_CAP_INTERNAL)) &&
             cJSON_AddNumberToObject(metrics, "free_psram",
                                     heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) &&
             cJSON_AddNumberToObject(metrics, "uptime_s", esp_timer_get_time() / 1000000) &&
             /* The device's own clock, so drift while the radio is off can be measured. */
             cJSON_AddNumberToObject(metrics, "time", (double)time(NULL)) &&
             cJSON_AddNumberToObject(metrics, "loop_wakes", home_loop_wakes()) &&
             cJSON_AddNumberToObject(metrics, "busy_polls", home_panel_busy_polls()) &&
             cJSON_AddNumberToObject(j, "config_revision", h->config.revision) &&
             cJSON_AddNumberToObject(j, "paired_clients", count) &&
             cJSON_AddNumberToObject(j, "pause_remaining",
                                     h->manual_until > esp_timer_get_time()
                                         ? (h->manual_until - esp_timer_get_time()) / 1000000
                                         : 0) &&
             cJSON_AddNumberToObject(j, "refresh_queued", h->refresh_requested);
        /* Power: without this there is no way to tell from outside a chip that sleeps from one
         * that only looks the same. */
        home_power_state_t pw;
        home_power_snapshot(&pw);
        cJSON *power = ok ? cJSON_AddObjectToObject(j, "power") : NULL;
        int64_t awake = h->awake_until - esp_timer_get_time();
        ok = ok && power &&
             cJSON_AddStringToObject(power, "mode",
                                     h->config.power_mode == HOME_POWER_OPEN ? "open" : "breath") &&
             cJSON_AddNumberToObject(power, "awake_seconds", awake > 0 ? awake / 1000000 : 0) &&
             cJSON_AddBoolToObject(power, "radio", home_network_radio_on()) &&
             cJSON_AddBoolToObject(power, "managed", pw.enabled) &&
             cJSON_AddBoolToObject(power, "locks_held", pw.locks_held) &&
             cJSON_AddNumberToObject(power, "reasons", pw.reasons) &&
             cJSON_AddNumberToObject(power, "held_s", (double)pw.held_s) &&
             cJSON_AddNumberToObject(power, "free_s", (double)pw.free_s);

        cJSON *sources = ok ? cJSON_AddObjectToObject(j, "sources") : NULL;
        ok = ok && sources && json_child(sources, "weather", meta_json(&h->data.weather.meta)) &&
             json_child(sources, "feed", feed_json(&h->data.feed)) &&
             json_child(sources, "air", meta_json(&h->data.air.meta));
    }
    home_unlock();
    if (!ok) {
        cJSON_Delete(j);
        return json_send(r, NULL);
    }
    if (token >= 0) {
        cJSON *wifi = home_network_status_json();
        if (!wifi || !cJSON_AddItemToObject(j, "wifi", wifi)) {
            cJSON_Delete(wifi);
            cJSON_Delete(j);
            return error(r, "503 Service Unavailable", "Status unavailable");
        }
    }
    return json_send(r, j);
}
static int query_screen(httpd_req_t *r)
{
    char q[96], s[16];
    if (httpd_req_get_url_query_str(r, q, sizeof(q)) != ESP_OK)
        return 0;
    if (httpd_query_key_value(q, "screen", s, sizeof(s)) != ESP_OK)
        return -1;
    return home_screen_index(s);
}
static esp_err_t preview(httpd_req_t *r, const char *draft, bool confirmed, bool sharing)
{
    int screen = query_screen(r);
    if (screen < 0)
        return error(r, "400 Bad Request", "Unknown screen");
    home_config_t *c = malloc(sizeof(*c));
    home_data_t *d = malloc(sizeof(*d));
    uint8_t *frame = malloc(HOME_FRAME_BYTES);
    if (!c || !d || !frame) {
        free(c);
        free(d);
        free(frame);
        return error(r, "503 Service Unavailable", "Preview unavailable");
    }
    home_lock();
    *c = home_runtime.config;
    *d = home_runtime.data;
    bool exists = home_runtime.frame_valid;
    if (sharing && home_runtime.displayed_screen < 0)
        exists = false; /* Setup frames contain access credentials, never export them. */
    uint32_t gen = home_runtime.generation;
    if (confirmed && exists)
        memcpy(frame, home_runtime.frame, HOME_FRAME_BYTES);
    home_unlock();
    if (confirmed && !exists) {
        free(c);
        free(d);
        free(frame);
        return error(r, "503 Service Unavailable",
                     sharing ? "Setup screens cannot be shared" : "No confirmed frame");
    }
    if (draft) {
        char why[128];
        home_config_t parsed;
        if (!home_config_decode(draft, strlen(draft), &parsed, c, false, why)) {
            free(c);
            free(d);
            free(frame);
            return error(r, "400 Bad Request", why);
        }
        if (parsed.latitude != c->latitude || parsed.longitude != c->longitude)
            memset(&d->weather, 0, sizeof(d->weather));
        if (strcmp(parsed.feed_url, c->feed_url))
            memset(&d->feed, 0, sizeof(d->feed));
        *c = parsed;
    }
    if (!confirmed) {
        time_t now = time(NULL);
        home_render_locked(c, d, screen, now >= 1704067200 ? now : 0, frame);
    }
    char generation[16];
    snprintf(generation, sizeof(generation), "%lu", (unsigned long)gen);
    httpd_resp_set_hdr(r, "X-Home-Generation", generation);
    httpd_resp_set_type(r, "application/octet-stream");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    esp_err_t e = httpd_resp_send(r, (char *)frame, HOME_FRAME_BYTES);
    free(c);
    free(d);
    free(frame);
    return e;
}
static esp_err_t api_inner(httpd_req_t *r)
{
    if (!origin_ok(r))
        return error(r, "403 Forbidden", "Local origin required");
    char path[80];
    size_t plen = strcspn(r->uri, "?");
    if (plen >= sizeof(path))
        return error(r, "404 Not Found", "Unknown endpoint");
    memcpy(path, r->uri, plen);
    path[plen] = 0;
    int token = auth(r);
    if (r->method == HTTP_GET && !strcmp(path, "/api/status"))
        return status(r, token);
    bool pairing = !strcmp(path, "/api/pair") && r->method == HTTP_POST;
    if (token < 0 && !pairing)
        return error(r, "401 Unauthorized", "Pair this phone with the code on Home");
    /* Breath mode: a paired phone in use keeps the device reachable - paired only, because a
     * pairing attempt passes the check above without a token (0.6.2). Not what the
     * panel reads by itself - the status (answered above), the picture after it changed, and
     * requests marked X-Home-Auto - or a tab left open would keep the radio on. */
    if (token >= 0 && !(r->method == HTTP_GET && !strcmp(path, "/api/frame")) &&
        !httpd_req_get_hdr_value_len(r, "X-Home-Auto")) {
        home_lock();
        home_runtime.awake_until = esp_timer_get_time() + HOME_AWAKE_US;
        home_unlock();
    }
    home_lock();
    bool maintenance = home_runtime.maintenance;
    home_unlock();
    if (maintenance && r->method != HTTP_GET)
        return error(r, "503 Service Unavailable", "Device maintenance in progress");
    if (r->method == HTTP_GET) {
        if (!strcmp(path, "/api/wifi/scan"))
            return json_send(r, home_network_scan_json());
        if (!strcmp(path, "/api/power"))
            return json_send(r, power_log_json());
#ifdef HOME_PM_PROFILE
        /* Diagnostic build: the power management lock table over the network, so it can be read
         * ON BATTERY. On a cable the chip does not sleep anyway (our lock plus the USB lock from
         * IDF), so a console snapshot cannot answer the question that matters here.
         * open_memstream is the approach suggested in the esp_pm.h header. */
        if (!strcmp(path, "/api/pm")) {
            char *bufor = NULL;
            size_t dlugosc = 0;
            FILE *strumien = open_memstream(&bufor, &dlugosc);
            if (!strumien)
                return error(r, "503 Service Unavailable", "out_of_memory");
            esp_pm_dump_locks(strumien);
            fclose(strumien);
            esp_err_t sent = httpd_resp_set_type(r, "text/plain");
            if (sent == ESP_OK)
                sent = httpd_resp_send(r, bufor ? bufor : "", HTTPD_RESP_USE_STRLEN);
            free(bufor);
            return sent;
        }
#endif
        if (!strcmp(path, "/api/timezones"))
            return json_send(r, home_timezones_json());
        if (!strcmp(path, "/api/location"))
            return json_send(r, home_network_location_json());
        if (!strcmp(path, "/api/config") || !strcmp(path, "/api/recipe")) {
            home_lock();
            cJSON *j = home_config_json(&home_runtime.config, !strcmp(path, "/api/recipe"));
            home_unlock();
            return json_send(r, j);
        }
        if (!strcmp(path, "/api/frame"))
            return preview(r, NULL, true, false);
        if (!strcmp(path, "/api/share-frame"))
            return preview(r, NULL, true, true);
        if (!strcmp(path, "/api/preview"))
            return preview(r, NULL, false, false);
        return error(r, "404 Not Found", "Unknown endpoint");
    }
    if (r->method != HTTP_POST && !(r->method == HTTP_PUT && !strcmp(path, "/api/config")))
        return error(r, "405 Method Not Allowed", "Method not allowed");
    char *b = body(r);
    if (!b)
        return error(r, "400 Bad Request", "Expected bounded JSON body");
    esp_err_t result = ESP_OK;
    cJSON *j = NULL;
    if (r->method == HTTP_POST && !strcmp(path, "/api/preview")) {
        result = preview(r, b, false, false);
        goto done;
    }
    if ((r->method == HTTP_PUT && !strcmp(path, "/api/config")) ||
        (r->method == HTTP_POST && !strcmp(path, "/api/recipe"))) {
        home_config_t c;
        char why[128];
        bool recipe = !strcmp(path, "/api/recipe");
        home_lock();
        if (!home_config_decode(b, strlen(b), &c, &home_runtime.config, recipe, why)) {
            home_unlock();
            result = error(r, "400 Bad Request", why);
            goto done;
        }
        if (!recipe && c.revision != home_runtime.config.revision) {
            home_unlock();
            result = error(r, "409 Conflict", "Settings changed; reload before saving");
            goto done;
        }
        c.revision = home_runtime.config.revision + 1;
        esp_err_t e = home_store_config(&c);
        if (e != ESP_OK) {
            home_unlock();
            result = error(r, "503 Service Unavailable", "Settings were not saved");
            goto done;
        }
        if (c.location_ready != home_runtime.config.location_ready ||
            c.latitude != home_runtime.config.latitude ||
            c.longitude != home_runtime.config.longitude) {
            memset(&home_runtime.data.weather, 0, sizeof(home_runtime.data.weather));
            memset(&home_runtime.data.air, 0, sizeof(home_runtime.data.air));
        }
        if (strcmp(c.feed_url, home_runtime.config.feed_url))
            memset(&home_runtime.data.feed, 0, sizeof(home_runtime.data.feed));
        home_runtime.refresh_requested &= home_sources_wanted(&c);
        home_runtime.config = c;
        home_runtime.request_id++; /* Save changes settings; explicit Show publishes them. */
        cJSON *out = home_config_json(&c, false);
        home_unlock();
        result = json_send(r, out);
        goto done;
    }
    j = small_json(b);
    if (!j) {
        result = error(r, "400 Bad Request", "Invalid JSON");
        goto done;
    }
    if (pairing) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "code");
        if (!only(j, "code", NULL) || !cJSON_IsString(v) || strlen(v->valuestring) != 6) {
            result = error(r, "400 Bad Request", "Six digit code required");
            goto done;
        }
        home_lock();
        if (attempt_window != home_runtime.pair_until) {
            attempt_window = home_runtime.pair_until;
            pair_attempts = 0;
        }
        if (esp_timer_get_time() >= home_runtime.pair_until || pair_attempts >= 5) {
            home_unlock();
            result = error(r, "429 Too Many Requests", "Hold OK on Home to open pairing again");
            goto done;
        }
        pair_attempts++;
        volatile unsigned diff = 0;
        for (int i = 0; i < 6; i++)
            diff |= (unsigned char)v->valuestring[i] ^ (unsigned char)home_runtime.pair_code[i];
        if (diff) {
            home_unlock();
            result = error(r, "403 Forbidden", "Code did not match");
            goto done;
        }
        uint8_t random[32], hash[32];
        char text[65];
        esp_fill_random(random, sizeof(random));
        for (int i = 0; i < 32; i++)
            snprintf(text + 2 * i, 3, "%02x", random[i]);
        if (!home_hash(text, 64, hash)) {
            home_unlock();
            result = error(r, "503 Service Unavailable", "Pairing unavailable");
            goto done;
        }
        home_secrets_t s = home_runtime.secrets;
        int slot = -1;
        for (int i = 0; i < 4; i++)
            if (!s.token_used[i]) {
                slot = i;
                break;
            }
        if (slot < 0) {
            home_unlock();
            memset(&s, 0, sizeof(s));
            memset(text, 0, sizeof(text));
            result = error(r, "409 Conflict", "pairing_limit_reached");
            goto done;
        }
        cJSON *out = cJSON_CreateObject();
        char *encoded = NULL;
        cJSON *value = cJSON_AddStringToObject(out, "token", text);
        if (value) {
            encoded = cJSON_PrintUnformatted(out);
            memset(value->valuestring, 0, strlen(value->valuestring));
        }
        cJSON_Delete(out);
        memset(text, 0, sizeof(text));
        if (!encoded) {
            home_unlock();
            memset(&s, 0, sizeof(s));
            result = error(r, "503 Service Unavailable", "Pairing unavailable");
            goto done;
        }
        memcpy(s.token_hash[slot], hash, 32);
        s.token_used[slot] = 1;
        if (home_store_secrets(&s) != ESP_OK) {
            home_unlock();
            memset(text, 0, sizeof(text));
            memset(encoded, 0, strlen(encoded));
            free(encoded);
            memset(&s, 0, sizeof(s));
            result = error(r, "503 Service Unavailable", "Pairing was not saved");
            goto done;
        }
        home_runtime.secrets = s;
        char host[96];
        header(r, "Host", host, sizeof(host));
        if (home_runtime.online && strncmp(host, "192.168.4.1", 11)) {
            home_runtime.setup = false;
            home_runtime.dirty = true;
            home_runtime.request_id++;
        }
        home_unlock();
        result = json_text(r, encoded);
        memset(encoded, 0, strlen(encoded));
        free(encoded);
        memset(text, 0, sizeof(text));
        memset(&s, 0, sizeof(s));
        goto done;
    }
    if (!strcmp(path, "/api/show")) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "screen");
        int s = cJSON_IsString(v) ? home_screen_index(v->valuestring) : -1;
        if (!only(j, "screen", NULL) || s < 0) {
            result = error(r, "400 Bad Request", "Choose a screen");
            goto done;
        }
        home_lock();
        if (home_runtime.phase == 3) {
            home_unlock();
            result = error(r, "503 Service Unavailable", "Display needs attention");
            goto done;
        }
        if (!home_runtime.config.enabled[s]) {
            home_unlock();
            result = error(r, "400 Bad Request", "Screen is disabled");
            goto done;
        }
        home_runtime.pending_manual = true;
        home_runtime.manual_id++;
        home_runtime.pending_screen = s;
        home_runtime.dirty = true;
        home_runtime.request_id++;
        home_runtime.setup = false;
        home_runtime.manual_until =
            esp_timer_get_time() + (int64_t)home_runtime.config.pause_min * 60000000;
        home_unlock();
        result = accepted(r);
        goto done;
    }
    if (!strcmp(path, "/api/pause")) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "minutes");
        if (!only(j, "minutes", NULL) || !cJSON_IsNumber(v) || !isfinite(v->valuedouble) ||
            v->valuedouble < 0 || v->valuedouble > 240 || floor(v->valuedouble) != v->valuedouble) {
            result = error(r, "400 Bad Request", "Invalid pause");
            goto done;
        }
        home_lock();
        home_runtime.manual_until = esp_timer_get_time() + (int64_t)v->valueint * 60000000;
        home_runtime.request_id++;
        home_unlock();
        result = accepted(r);
        goto done;
    }
    if (!strcmp(path, "/api/refresh")) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "source");
        if (!only(j, "source", NULL) || !cJSON_IsString(v) ||
            (strcmp(v->valuestring, "weather") && strcmp(v->valuestring, "feed") &&
             strcmp(v->valuestring, "air") && strcmp(v->valuestring, "all"))) {
            result = error(r, "400 Bad Request", "Unknown source");
            goto done;
        }
        unsigned mask = !strcmp(v->valuestring, "weather") ? 1U
                        : !strcmp(v->valuestring, "feed")  ? 2U
                        : !strcmp(v->valuestring, "air")   ? 4U
                                                           : 7U;
        home_lock();
        /* Only a source whose screen is on and has a place or an address is asked. */
        mask &= home_sources_wanted(&home_runtime.config);
        home_runtime.refresh_requested |= mask;
        home_unlock();
        /* A code, so the panel can say why in the reader's language. */
        result = mask ? accepted(r) : error(r, "400 Bad Request", "source_off");
        goto done;
    }
    if (!strcmp(path, "/api/wifi")) {
        cJSON *s = cJSON_GetObjectItemCaseSensitive(j, "ssid"),
              *p = cJSON_GetObjectItemCaseSensitive(j, "password");
        if (!only(j, "ssid", "password") || !cJSON_IsString(s) || !cJSON_IsString(p) ||
            !home_utf8(s->valuestring, 32, false) || !s->valuestring[0] ||
            !home_utf8(p->valuestring, 63, false) || strlen(p->valuestring) < 8) {
            result = error(r, "400 Bad Request", "WPA2 network and 8-63 byte password required");
            goto done;
        }
        esp_err_t e = home_network_credentials(s->valuestring, p->valuestring);
        memset(p->valuestring, 0, strlen(p->valuestring));
        result = e == ESP_OK ? accepted(r)
                             : error(r, "503 Service Unavailable", "Wi-Fi settings not saved");
        goto done;
    }
    if (!strcmp(path, "/api/wifi/scan")) {
        if (!cJSON_IsObject(j) || j->child) {
            result = error(r, "400 Bad Request", "Expected empty object");
            goto done;
        }
        result = home_network_scan_start() == ESP_OK
                     ? accepted(r)
                     : error(r, "429 Too Many Requests", "Wait a moment before scanning again");
        goto done;
    }
    if (!strcmp(path, "/api/location")) {
        if (!cJSON_IsObject(j) || j->child) {
            result = error(r, "400 Bad Request", "Expected empty object");
            goto done;
        }
        const char *why = NULL;
        if (home_network_location_start(&why) != ESP_OK) {
            result =
                error(r,
                      why && (!strcmp(why, "location_cooldown") || !strcmp(why, "location_busy"))
                          ? "429 Too Many Requests"
                          : "503 Service Unavailable",
                      why ? why : "location_unavailable");
            goto done;
        }
        httpd_resp_set_status(r, "202 Accepted");
        result = json_send(r, home_network_location_json());
        goto done;
    }
    if (!strcmp(path, "/api/unpair")) {
        if (!cJSON_IsObject(j) || j->child) {
            result = error(r, "400 Bad Request", "Expected empty object");
            goto done;
        }
        home_lock();
        home_secrets_t s = home_runtime.secrets;
        memset(s.token_hash[token], 0, 32);
        s.token_used[token] = 0;
        esp_err_t e = home_store_secrets(&s);
        if (e == ESP_OK)
            home_runtime.secrets = s;
        home_unlock();
        memset(&s, 0, sizeof(s));
        result = e == ESP_OK ? accepted(r) : error(r, "503 Service Unavailable", "Unpair failed");
        goto done;
    }
    result = error(r, "404 Not Found", "Unknown endpoint");
done:
    if (j)
        cJSON_Delete(j);
    memset(b, 0, strlen(b));
    free(b);
    return result;
}
static esp_err_t api(httpd_req_t *r)
{
    home_lock();
    if (home_runtime.maintenance && r->method != HTTP_GET) {
        home_unlock();
        return error(r, "503 Service Unavailable", "Device maintenance in progress");
    }
    home_runtime.api_active++;
    home_unlock();
    esp_err_t result = api_inner(r);
    home_lock();
    home_runtime.api_active--;
    home_unlock();
    return result;
}
static esp_err_t asset(httpd_req_t *r)
{
    if (!origin_ok(r))
        return error(r, "403 Forbidden", "Local origin required");
    const uint8_t *start = NULL, *end = NULL;
    const char *type = "text/html; charset=utf-8";
    if (!strcmp(r->uri, "/") || !strcmp(r->uri, "/index.html")) {
        start = index_html_start;
        end = index_html_end;
    } else if (!strcmp(r->uri, "/device.css")) {
        start = device_css_start;
        end = device_css_end;
        type = "text/css";
    } else if (!strcmp(r->uri, "/device.js")) {
        start = device_js_start;
        end = device_js_end;
        type = "application/javascript";
    } else if (!strcmp(r->uri, "/core.js")) {
        start = core_js_start;
        end = core_js_end;
        type = "application/javascript";
    } else
        return error(r, "404 Not Found", "Not found");
    httpd_resp_set_type(r, type);
    httpd_resp_set_hdr(r, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(
        r, "Content-Security-Policy",
        "default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data: blob:; "
        "connect-src 'self' https://geocoding-api.open-meteo.com; base-uri 'none'; frame-ancestors 'none'; form-action 'self'");
    httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(r, "Referrer-Policy", "no-referrer");
    return httpd_resp_send(r, (const char *)start, end - start);
}
esp_err_t home_server_start(void)
{
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.uri_match_fn = httpd_uri_match_wildcard;
    c.max_uri_handlers = 8;
    c.stack_size = 16384;
    c.max_open_sockets = 5;
    c.lru_purge_enable = true;
    c.recv_wait_timeout = 2;
    c.send_wait_timeout = 3;
    esp_err_t e = httpd_start(&server, &c);
    if (e != ESP_OK)
        return e;
    httpd_uri_t a = {.uri = "/api/*", .method = HTTP_GET, .handler = api};
    if ((e = httpd_register_uri_handler(server, &a)) != ESP_OK)
        return e;
    a.method = HTTP_POST;
    if ((e = httpd_register_uri_handler(server, &a)) != ESP_OK)
        return e;
    a.method = HTTP_PUT;
    if ((e = httpd_register_uri_handler(server, &a)) != ESP_OK)
        return e;
    httpd_uri_t s = {.uri = "/*", .method = HTTP_GET, .handler = asset};
    return httpd_register_uri_handler(server, &s);
}
