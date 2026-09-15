#include "home_runtime.h"
#include "home_config.h"
#include "home_places.h"
#include "home_discovery.h"
#include "home_panel.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_app_desc.h"
#include "bootloader_random.h"
#include "psa/crypto.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

home_runtime_t home_runtime;
static SemaphoreHandle_t render_lock;
static const char *TAG = "home3";
void home_lock(void)
{
    xSemaphoreTake(home_runtime.lock, portMAX_DELAY);
}
void home_unlock(void)
{
    xSemaphoreGive(home_runtime.lock);
}
bool home_hash(const void *p, size_t n, uint8_t out[32])
{
    size_t actual = 0;
    return psa_hash_compute(PSA_ALG_SHA_256, p, n, out, 32, &actual) == PSA_SUCCESS && actual == 32;
}
void home_render_locked(const home_config_t *c, const home_data_t *d, int screen, int64_t now,
                        uint8_t *frame)
{
    xSemaphoreTake(render_lock, portMAX_DELAY);
    home_render(c, d, screen, now, frame);
    xSemaphoreGive(render_lock);
}
bool home_localtime(const home_config_t *c, time_t now, struct tm *out)
{
    return home_tz_localtime(c->timezone, (int64_t)now, out);
}
void home_begin_pairing(void)
{
    home_lock();
    if (home_runtime.maintenance) {
        home_unlock();
        return;
    }
    snprintf(home_runtime.pair_code, sizeof(home_runtime.pair_code), "%06lu",
             (unsigned long)(esp_random() % 1000000));
    home_runtime.pair_until = esp_timer_get_time() + INT64_C(300000000);
    home_runtime.setup = true;
    home_runtime.dirty = true;
    home_runtime.request_id++;
    home_unlock();
    ESP_LOGI(TAG, "Physical pairing window opened for five minutes");
}
static void output(gpio_num_t pin, int value)
{
    ESP_ERROR_CHECK(gpio_hold_dis(pin));
    ESP_ERROR_CHECK(gpio_set_level(pin, value));
    gpio_config_t c = {.pin_bit_mask = 1ULL << pin, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&c));
    ESP_ERROR_CHECK(gpio_set_level(pin, value));
    ESP_ERROR_CHECK(gpio_hold_en(pin));
}
static int keys(void)
{
    return (!gpio_get_level(GPIO_NUM_39) ? 1 : 0) | (!gpio_get_level(GPIO_NUM_18) ? 2 : 0) |
           (!gpio_get_level(GPIO_NUM_0) ? 4 : 0);
}
/* "In turn" compositions: appearances per screen and when the current one started.
 * Only the main task (loop and action) touches these. */
static uint32_t cycle_showing[HOME_SCREEN_COUNT];
static int64_t cycle_at[HOME_SCREEN_COUNT];
/* A screen whose card does not depend on the composition (no data yet). */
static bool screen_has_data(int s)
{
    const home_config_t *c = &home_runtime.config;
    const home_data_t *d = &home_runtime.data;
    switch (s) {
    case HOME_WEATHER:
        return c->location_ready && d->weather.meta.valid;
    case HOME_FEED:
        return d->feed.meta.valid;
    case HOME_NOTE:
        return c->note[0] != 0;
    case HOME_SKY:
        return c->location_ready; /* computed on the device, nothing to download */
    case HOME_AIR:
        return c->location_ready && d->air.meta.valid;
    default:
        return false;
    }
}
static void action(int key)
{
    int64_t now = esp_timer_get_time();
    home_lock();
    if (home_runtime.maintenance) {
        home_unlock();
        return;
    }
    int current = home_runtime.pending_screen >= 0 ? home_runtime.pending_screen
                                                   : home_runtime.displayed_screen;
    if (current < 0)
        current = 0;
    bool pause = true; /* a press normally holds automatic changes for pause_min */
    if (key == 4) {
        /* Short OK/BOOT: the job chosen in the panel (Settings > Preferences). */
        uint8_t what = home_runtime.config.ok_action;
        if (what == 3) { /* setup window, like the long press */
            home_unlock();
            home_begin_pairing();
            ESP_LOGI(TAG, "Physical short release key=4: setup window");
            return;
        } else if (what == 1) { /* fetch weather, the headline and the air now */
            home_runtime.refresh_requested |= 1U;
            if (home_runtime.config.feed_url[0])
                home_runtime.refresh_requested |= 2U;
            if (home_runtime.config.enabled[HOME_AIR] && home_runtime.config.location_ready)
                home_runtime.refresh_requested |= 4U;
            pause = false;
        } else if (what == 2) { /* hold the current screen, or resume when already held */
            if (home_runtime.manual_until > now) {
                home_runtime.manual_until = now;
                pause = false;
            }
        } else {
            home_config_t c = home_runtime.config;
            strcpy(c.locale, !strcmp(c.locale, "en") ? "pl" : "en");
            c.revision++;
            if (home_store_config(&c) == ESP_OK)
                home_runtime.config = c;
        }
    } else {
        /* On an "In turn" screen, Down and Up first walk through its three compositions. */
        int shown = home_runtime.displayed_screen;
        int step = key == 2 ? 1 : -1;
        int showing =
            shown >= 0 && cycle_showing[shown] ? (int)((cycle_showing[shown] - 1) % 3) : 0;
        if (shown >= 0 && shown < HOME_SCREEN_COUNT && shown == current &&
            home_runtime.config.style[shown] == HOME_CYCLE && screen_has_data(shown) &&
            showing + step >= 0 && showing + step <= 2) {
            cycle_showing[shown] = (uint32_t)(showing + step + 1);
            cycle_at[shown] = now;
            home_runtime.pending_screen = shown;
        } else {
            int pos = 0;
            for (int i = 0; i < HOME_SCREEN_COUNT; i++)
                if (home_runtime.config.order[i] == current)
                    pos = i;
            for (int n = 1; n <= HOME_SCREEN_COUNT; n++) {
                int pick =
                    home_runtime.config
                        .order[(pos + (key == 2 ? n : HOME_SCREEN_COUNT - n)) % HOME_SCREEN_COUNT];
                if (home_runtime.config.enabled[pick]) {
                    home_runtime.pending_screen = pick;
                    if (home_runtime.config.style[pick] == HOME_CYCLE) {
                        /* Enter from the matching end: Print going down, Atlas going up.
                         * A screen new to the display counts one more appearance when drawn. */
                        cycle_showing[pick] = (key == 2 ? 1 : 3) - (pick != shown);
                        cycle_at[pick] = now;
                    }
                    break;
                }
            }
        }
    }
    home_runtime.pending_manual = true;
    home_runtime.manual_id++;
    home_runtime.setup = false;
    home_runtime.dirty = true;
    home_runtime.request_id++;
    if (pause)
        home_runtime.manual_until = now + (int64_t)home_runtime.config.pause_min * 60000000;
    home_unlock();
    ESP_LOGI(TAG, "Physical short release key=%d", key);
}
void app_main(void)
{
    output(GPIO_NUM_17, 1);
    output(GPIO_NUM_21, 0);
    output(GPIO_NUM_42, 0);
    output(GPIO_NUM_46, 0);
    gpio_config_t buttons = {.pin_bit_mask = (1ULL << 39) | (1ULL << 18) | 1ULL,
                             .mode = GPIO_MODE_INPUT,
                             .pull_up_en = GPIO_PULLUP_ENABLE};
    ESP_ERROR_CHECK(gpio_config(&buttons));
    home_runtime.lock = xSemaphoreCreateMutex();
    render_lock = xSemaphoreCreateMutex();
    /* Explicit checks, not assert(): these must also run with assertions disabled. */
    if (!home_runtime.lock || !render_lock) {
        ESP_LOGE(TAG, "Lock allocation failed");
        abort();
    }
    home_runtime.frame = heap_caps_malloc(HOME_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *work = heap_caps_malloc(HOME_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    home_config_t *c = malloc(sizeof(*c));
    home_data_t *d = malloc(sizeof(*d));
    if (!home_runtime.frame || !work || !c || !d) {
        ESP_LOGE(TAG, "Frame buffer allocation failed");
        abort();
    }
    ESP_ERROR_CHECK(
        home_store_init(&home_runtime.config, &home_runtime.data, &home_runtime.secrets));
    if (psa_crypto_init() != PSA_SUCCESS) {
        ESP_LOGE(TAG, "Crypto initialization failed");
        abort();
    }
    if (!home_runtime.secrets.ap_password[0]) {
        bootloader_random_enable();
        const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        for (int i = 0; i < 12; i++)
            home_runtime.secrets.ap_password[i] = alphabet[esp_random() % 32];
        snprintf(home_runtime.secrets.ap_ssid, sizeof(home_runtime.secrets.ap_ssid), "Home-%04lX",
                 (unsigned long)(esp_random() & 65535));
        bootloader_random_disable();
        ESP_ERROR_CHECK(home_store_secrets(&home_runtime.secrets));
    }
    /* Public AP branding is separate from the persisted per-unit mDNS identity. */
    strcpy(home_runtime.ssid, "emini.ink");
    strcpy(home_runtime.address, "192.168.4.1");
    home_runtime.displayed_screen = -1;
    home_runtime.pending_screen = home_runtime.config.fixed_screen;
    home_runtime.dirty = true;
    home_runtime.request_id++;
    ESP_ERROR_CHECK(home_panel_init());
#ifdef HOME_TESTCARD
    /* Measurement build (plan 0.5.0 step 0.1): show the test cards and stop
     * here, before any network. Any key advances to the next card. */
    for (int card = 0;; card = (card + 1) % HOME_TESTCARDS) {
        home_render_testcard(card, work);
        ESP_LOGI(TAG, "TESTCARD %d of %d", card + 1, HOME_TESTCARDS);
        esp_err_t shown = home_panel_show(work, HOME_FRAME_BYTES);
        if (shown != ESP_OK) {
            ESP_LOGE(TAG, "TESTCARD panel refresh failed: %s", esp_err_to_name(shown));
            for (;;)
                vTaskDelay(portMAX_DELAY);
        }
        while (keys())
            vTaskDelay(pdMS_TO_TICKS(50));
        do {
            vTaskDelay(pdMS_TO_TICKS(50));
        } while (!keys());
        vTaskDelay(pdMS_TO_TICKS(50));
        while (keys())
            vTaskDelay(pdMS_TO_TICKS(50));
    }
#endif
    ESP_ERROR_CHECK(home_network_start());
    esp_err_t discovery = home_discovery_start(home_runtime.secrets.ap_ssid);
    if (discovery == ESP_OK) {
        char base[33];
        snprintf(base, sizeof(base), "%s", home_runtime.secrets.ap_ssid);
        for (size_t i = 0; base[i]; ++i)
            base[i] = (char)tolower((unsigned char)base[i]);
        snprintf(home_runtime.hostname, sizeof(home_runtime.hostname), "%s.local", base);
    } else {
        ESP_LOGW(TAG, "mDNS unavailable; numeric IP remains available");
    }
    ESP_ERROR_CHECK(home_server_start());
    ESP_ERROR_CHECK(home_usb_start());
    bool paired = false;
    for (int i = 0; i < 4; i++)
        paired |= home_runtime.secrets.token_used[i] != 0;
    if (!home_runtime.secrets.ssid[0] || !paired)
        home_begin_pairing();
    else
        home_runtime.manual_until = esp_timer_get_time();
    if (xTaskCreate(home_sources_task, "home_sources", 24576, NULL, 4, NULL) != pdPASS ||
        xTaskCreate(home_battery_task, "home_battery", 3072, NULL, 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Task creation failed");
        abort();
    }
    ESP_LOGI(TAG, "BOOT emini_home_g3 %s; local panel active; stock NVS untouched",
             esp_app_get_description()->version);
    int prev = keys(), candidate = 0;
    bool armed = false, long_fired = false;
    int64_t stable = esp_timer_get_time(), pressed = 0, last_status = 0, last_minute = -1;
    uint8_t last_hash[32] = {0};
    bool have_hash = false;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(20));
        int64_t mono = esp_timer_get_time();
        time_t now = time(NULL);
        int raw = keys();
        if (raw != prev) {
            prev = raw;
            stable = mono;
        }
        if (mono - stable >= 50000) {
            if (!armed) {
                if (raw == 0) {
                    armed = true;
                    candidate = 0;
                    long_fired = false;
                }
            } else if (raw && (raw & (raw - 1))) {
                armed = false;
                candidate = 0;
            } else if (raw && !candidate) {
                candidate = raw;
                pressed = mono;
            } else if (raw == 4 && candidate == 4 && !long_fired && mono - pressed >= 2000000) {
                home_begin_pairing();
                long_fired = true;
                armed = false;
                candidate = 0;
            } else if (raw && raw != candidate) {
                armed = false;
                candidate = 0;
            } else if (candidate && raw == 0) {
                if (!long_fired && mono - pressed <= 1500000)
                    action(candidate);
                candidate = 0;
            }
        }
        home_lock();
        *c = home_runtime.config;
        *d = home_runtime.data;
        bool dirty = home_runtime.dirty, setup = home_runtime.setup;
        int screen = home_runtime.pending_screen;
        uint64_t request_id = home_runtime.request_id, manual_id = home_runtime.manual_id;
        bool manual_request = home_runtime.pending_manual;
        int current = home_runtime.displayed_screen;
        int phase = home_runtime.phase;
        int64_t manual = home_runtime.manual_until, last_switch = home_runtime.last_switch;
        bool clock_synced = home_runtime.time_valid; /* SNTP has set the clock (sources task) */
        home_unlock();
        struct tm local;
        bool valid = clock_synced && now >= 1704067200 && home_localtime(c, now, &local);
        bool quiet = valid && home_is_quiet(c, &local);
        if (!setup && screen < 0)
            screen = current >= 0 ? current : 0;
        if (!setup && !manual_request && mono >= manual && !quiet) {
            int desired = home_auto_screen(c, d, valid ? &local : NULL, screen,
                                           (mono - last_switch) / 1000000);
            if (desired != screen) {
                screen = desired;
                dirty = true;
            }
        }
        if (valid && now / 60 != last_minute) {
            last_minute = now / 60;
            home_lock();
            home_source_meta_t *meta[] = {&home_runtime.data.weather.meta,
                                          &home_runtime.data.feed.meta,
                                          &home_runtime.data.air.meta};
            for (int i = 0; i < 3; i++)
                if (meta[i]->valid && meta[i]->expires_at < now && meta[i]->state == HOME_READY) {
                    meta[i]->state = HOME_STALE;
                    home_runtime.dirty = true;
                    home_runtime.request_id++;
                    dirty = true;
                }
            *d = home_runtime.data;
            home_unlock();
            if (now % 3600 < 60)
                dirty = true;
        }
        /* "In turn": the next composition whenever the screen appears, and every
         * cycle_min while it stays on the display. The timer waits for valid time,
         * the manual pause and quiet hours. Choosing In turn for the screen on the
         * display starts the timer without redrawing (Save does not publish). */
        bool cycle =
            !setup && screen >= 0 && screen < HOME_SCREEN_COUNT && c->style[screen] == HOME_CYCLE;
        if (cycle && screen == current && !cycle_at[screen]) {
            cycle_at[screen] = mono;
            cycle_showing[screen] = 1;
        }
        bool cycle_due = cycle && (screen != current ||
                                   (valid && mono >= manual &&
                                    mono - cycle_at[screen] >= (int64_t)c->cycle_min * 60000000));
        if (cycle_due && screen == current && !quiet)
            dirty = true;
        if (phase != 3 && dirty &&
            (!quiet || setup || manual_request || mono < manual || !home_runtime.frame_valid)) {
            char ssid[33], pass[17], code[7], address[48];
            home_lock();
            if (home_runtime.request_id != request_id ||
                home_runtime.config.revision != c->revision || home_runtime.maintenance) {
                home_unlock();
                continue;
            }
            home_runtime.dirty = false;
            home_runtime.phase = 1;
            home_runtime.pending_screen = screen;
            snprintf(ssid, sizeof(ssid), "%s", home_runtime.ssid);
            snprintf(pass, sizeof(pass), "%s", home_runtime.secrets.ap_password);
            snprintf(code, sizeof(code), "%s", home_runtime.pair_code);
            /* Setup first joins our AP, regardless of the station connection. */
            snprintf(address, sizeof(address), "http://192.168.4.1");
            home_unlock();
            int64_t render_start = esp_timer_get_time();
            if (cycle_due) {
                cycle_showing[screen]++;
                cycle_at[screen] = mono;
            }
            if (cycle)
                c->style[screen] = (uint8_t)home_style_for(c, screen, cycle_showing[screen]);
            if (setup)
                home_render_setup(ssid, pass, code, address, !strcmp(c->locale, "pl"), work);
            else
                home_render_locked(c, d, screen, valid ? now : 0, work);
            home_lock();
            home_runtime.render_ms = (esp_timer_get_time() - render_start) / 1000;
            home_unlock();
            memset(pass, 0, sizeof(pass));
            memset(code, 0, sizeof(code));
            uint8_t hash[32];
            if (!home_hash(work, HOME_FRAME_BYTES, hash)) {
                ESP_LOGE(TAG, "Frame hash failure");
                home_lock();
                home_runtime.phase = 3;
                home_unlock();
                continue;
            }
            if (have_hash && !memcmp(hash, last_hash, 32)) {
                home_lock();
                home_runtime.phase = 0;
                if (home_runtime.request_id == request_id)
                    home_runtime.pending_screen = -1;
                if (home_runtime.manual_id == manual_id)
                    home_runtime.pending_manual = false;
                if (home_runtime.displayed_screen != (setup ? -1 : screen))
                    home_runtime.last_switch = esp_timer_get_time();
                home_runtime.displayed_screen = setup ? -1 : screen;
                home_unlock();
                continue;
            }
            home_lock();
            home_runtime.phase = 2;
            home_unlock();
            int64_t start = esp_timer_get_time();
            esp_err_t e = home_panel_show(work, HOME_FRAME_BYTES);
            home_lock();
            home_runtime.refresh_ms = (esp_timer_get_time() - start) / 1000;
            if (e == ESP_OK) {
                memcpy(home_runtime.frame, work, HOME_FRAME_BYTES);
                memcpy(last_hash, hash, 32);
                have_hash = true;
                home_runtime.frame_valid = true;
                if (home_runtime.displayed_screen != (setup ? -1 : screen))
                    home_runtime.last_switch = esp_timer_get_time();
                home_runtime.displayed_screen = setup ? -1 : screen;
                home_runtime.generation++;
                home_runtime.phase = 0;
                if (home_runtime.request_id == request_id)
                    home_runtime.pending_screen = -1;
                if (home_runtime.manual_id == manual_id)
                    home_runtime.pending_manual = false;
            } else
                home_runtime.phase = 3;
            ESP_LOGI(TAG,
                     "DISPLAY state=%s generation=%lu screen=%s refresh_ms=%lu render_ms=%lu "
                     "heap=%lu psram=%lu stack_min=%lu",
                     e == ESP_OK ? "ready" : "error", (unsigned long)home_runtime.generation,
                     setup ? "setup" : home_screen_name(screen),
                     (unsigned long)home_runtime.refresh_ms, (unsigned long)home_runtime.render_ms,
                     (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                     (unsigned long)uxTaskGetStackHighWaterMark(NULL));
            home_unlock();
            armed = false;
            candidate = 0;
            long_fired = false;
            prev = keys();
            stable = esp_timer_get_time();
        }
        if (mono - last_status >= 30000000) {
            last_status = mono;
            home_lock();
            ESP_LOGI(TAG, "STATUS generation=%lu screen=%s online=%d phase=%d uptime_s=%lld",
                     (unsigned long)home_runtime.generation,
                     home_runtime.displayed_screen < 0
                         ? "setup"
                         : home_screen_name(home_runtime.displayed_screen),
                     home_runtime.online, home_runtime.phase, (long long)(mono / 1000000));
            home_unlock();
        }
    }
}
