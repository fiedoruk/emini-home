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
/* One picture of what the device knows about itself, for the "emini" card. Caller holds the lock;
 * the estimate comes from the device's own week, so it appears only once there is a real slope. */
void home_stats_snapshot(home_stats_t *out, int64_t now)
{
    const home_counters_t *n = &home_runtime.counters;
    memset(out, 0, sizeof(*out));
    out->first_start = n->first_start;
    out->pictures = n->pictures;
    out->fetches = n->fetches;
    out->awake_hours = n->awake_minutes / 60;
    out->render_ms = home_runtime.render_ms;
    out->refresh_ms = home_runtime.refresh_ms;
    memcpy(out->battery_day, n->battery_day, sizeof(out->battery_day));
    out->percent = home_runtime.battery.percent_estimate;
    out->charging = home_runtime.battery.charging;
    out->full = home_runtime.battery.full;
    out->estimate_hours = -1;
    snprintf(out->address, sizeof(out->address), "%s",
             home_runtime.hostname[0] ? home_runtime.hostname : home_runtime.address);
    /* Slope from the oldest day we still have to today: percent per day, then hours left. */
    int last = -1;
    for (int k = HOME_BATTERY_DAYS - 1; k > 0; --k)
        if (n->battery_day[k] >= 0) {
            last = k;
            break;
        }
    if (out->percent >= 0 && !out->charging && last > 0 && n->battery_day[last] > out->percent) {
        int drop = n->battery_day[last] - out->percent;
        int hours = out->percent * 24 * last / drop;
        out->estimate_hours = hours > 24 * 60 ? 24 * 60 : hours;
    }
    (void)now;
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
/* Held presses. OK/BOOT after two seconds opens the setup window (it always has). Up after two
 * seconds holds the picture, Down after five switches the display language, so the two jobs people
 * reach for most no longer need the panel (D-HOME-CC-28). A press held for two to five seconds and
 * then let go asks the sources for fresh data; see the release path in the loop. */
static void long_action(int key)
{
    if (key == 4) {
        home_begin_pairing();
        ESP_LOGI(TAG, "Physical hold key=4: setup window");
        return;
    }
    int64_t now = esp_timer_get_time();
    home_lock();
    if (home_runtime.maintenance) {
        home_unlock();
        return;
    }
    if (key == 1) { /* hold the picture, or let it move again */
        bool holding = home_runtime.manual_until > now;
        home_runtime.manual_until =
            holding ? now : now + (int64_t)home_runtime.config.pause_min * 60000000;
        home_runtime.info_until = 0;
        home_runtime.dirty = true;
        home_runtime.request_id++;
        home_unlock();
        ESP_LOGI(TAG, "Physical hold key=1: %s", holding ? "resume" : "hold");
        return;
    }
    home_config_t c = home_runtime.config; /* key 2: the next display language, in a ring */
    strcpy(c.locale, !strcmp(c.locale, "en") ? "pl" : !strcmp(c.locale, "pl") ? "zh" : "en");
    c.revision++;
    bool saved = home_store_config(&c) == ESP_OK;
    if (saved) {
        home_runtime.config = c;
        home_runtime.dirty = true;
        home_runtime.request_id++;
        /* Hold the picture like any other press does. Without this the automatic change of
         * screens can fall due in the same second and the reader sees the next screen instead
         * of the language they just asked for (bug report 16.09). */
        home_runtime.manual_until = now + (int64_t)c.pause_min * 60000000;
    }
    home_unlock();
    ESP_LOGI(TAG, "Physical hold key=2: language %s", saved ? c.locale : "not saved");
}
/* Let go of a side button after two seconds: ask every source for fresh data. */
static void refresh_action(void)
{
    home_lock();
    if (!home_runtime.maintenance) {
        home_runtime.refresh_requested |= 1U;
        if (home_runtime.config.feed_url[0])
            home_runtime.refresh_requested |= 2U;
        if (home_runtime.config.enabled[HOME_AIR] && home_runtime.config.location_ready)
            home_runtime.refresh_requested |= 4U;
        /* The fresh data belongs on the screen the reader is looking at, and the reader has
         * to see that the press did something even when the provider answers "not modified"
         * and every pixel stays where it was. */
        home_runtime.manual_until =
            esp_timer_get_time() + (int64_t)home_runtime.config.pause_min * 60000000;
        home_runtime.force_show = true;
    }
    home_unlock();
    ESP_LOGI(TAG, "Physical hold-and-release: refresh requested");
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
            home_runtime.force_show = true;
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
        } else { /* the "emini" card, for two minutes, or dismiss it when it is up */
            home_runtime.info_until = home_runtime.info_until > now ? 0 : now + INT64_C(120000000);
            home_runtime.dirty = true;
            home_runtime.request_id++;
            pause = false;
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
    /* While the setup window is open the card is the only place the code and the password are
     * readable, so a press must not take them away (QC first start, 16.09). */
    if (esp_timer_get_time() >= home_runtime.pair_until)
        home_runtime.setup = false;
    home_runtime.dirty = true;
    home_runtime.request_id++;
    if (pause)
        home_runtime.manual_until = now + (int64_t)home_runtime.config.pause_min * 60000000;
    home_unlock();
    ESP_LOGI(TAG, "Physical short release key=%d", key);
}
/* One key at a time, timed with absolute clocks. The loop samples every 20 ms and the panel
 * calls the same tick every 50 ms while a picture is on its way, because a refresh takes some
 * 25 seconds and until 0.5.1 every press made during one was thrown away. A gap between two
 * samples therefore never shortens a hold, and a hold whose threshold falls inside such a gap
 * is still recognised when the key comes back up.
 * A dropout shorter than the release window counts as noise, not as letting go: the Down key
 * shares its line with the power key of the board (`VBAT_PWR_GPIO` = GPIO18 in the vendor
 * header), so its level is less clean than the other two. */
#define KEY_SETTLE_US INT64_C(50000)
#define KEY_RELEASE_US INT64_C(150000)
#define KEY_SHORT_US INT64_C(1500000)
static struct {
    int raw, candidate;
    int64_t changed, pressed;
    bool armed, long_fired;
} key_state;
static int64_t key_hold_us(int key)
{
    return key == 2 ? INT64_C(5000000) : INT64_C(2000000); /* Down: five seconds, D-HOME-CC-28 */
}
/* What the device made of the press, for the log and for /api/status: a reader can check
 * whether a gesture registered without a cable. */
static void key_seen(int key, int64_t held, const char *what)
{
    home_lock();
    home_runtime.key_last = key;
    home_runtime.key_last_ms = (int)(held / 1000);
    home_runtime.key_last_what = what;
    home_unlock();
    ESP_LOGI(TAG, "KEY key=%d held_ms=%d action=%s", key, (int)(held / 1000), what);
}
static void buttons_tick(void)
{
    int64_t mono = esp_timer_get_time();
    int raw = keys();
    if (raw != key_state.raw) {
        key_state.raw = raw;
        key_state.changed = mono;
    }
    int64_t settle = key_state.candidate && !raw ? KEY_RELEASE_US : KEY_SETTLE_US;
    if (mono - key_state.changed < settle)
        return;
    if (!key_state.armed) { /* wait for a clean release before timing anything new */
        if (!raw) {
            key_state.armed = true;
            key_state.candidate = 0;
            key_state.long_fired = false;
        }
        return;
    }
    if (raw && (raw & (raw - 1))) { /* two keys at once is not a gesture */
        key_state.armed = false;
        key_state.candidate = 0;
        return;
    }
    if (raw && !key_state.candidate) {
        key_state.candidate = raw;
        key_state.pressed = key_state.changed;
        return;
    }
    if (raw && raw != key_state.candidate) {
        key_state.armed = false;
        key_state.candidate = 0;
        return;
    }
    if (raw) { /* still down */
        if (!key_state.long_fired && mono - key_state.pressed >= key_hold_us(key_state.candidate)) {
            key_seen(key_state.candidate, mono - key_state.pressed, "hold");
            long_action(key_state.candidate);
            key_state.long_fired = true;
            key_state.armed = false;
            key_state.candidate = 0;
        }
        return;
    }
    if (!key_state.candidate)
        return;
    int key = key_state.candidate;
    int64_t held = key_state.changed - key_state.pressed; /* to the moment the line came up */
    key_state.candidate = 0;
    if (key_state.long_fired)
        return;
    if (held >= key_hold_us(key)) {
        key_seen(key, held, "hold"); /* the threshold fell between two samples */
        long_action(key);
    } else if (held <= KEY_SHORT_US) {
        key_seen(key, held, "press");
        action(key);
    } else if (key == 2 && held >= INT64_C(2000000)) {
        key_seen(key, held, "refresh");
        refresh_action();
    } else
        key_seen(key, held, "none"); /* between the windows: nothing to do, but it is on record */
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
    /* Counters for the "emini" card. A missing or foreign record simply starts them at zero. */
    memset(&home_runtime.counters, 0, sizeof(home_runtime.counters));
    for (int k = 0; k < HOME_BATTERY_DAYS; ++k)
        home_runtime.counters.battery_day[k] = -1;
    if (home_store_stats_load(&home_runtime.counters) != ESP_OK)
        ESP_LOGI(TAG, "Counters start from zero");
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
    home_panel_set_idle_hook(buttons_tick); /* keep watching the keys during the long refresh */
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
    key_state.raw = keys();
    key_state.changed = esp_timer_get_time();
    int64_t last_status = 0, last_minute = -1;
    bool drew_setup = false; /* the picture on the display is the setup card */
    uint8_t last_hash[32] = {0};
    bool have_hash = false;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(20));
        int64_t mono = esp_timer_get_time();
        time_t now = time(NULL);
        buttons_tick();
        home_lock();
        *c = home_runtime.config;
        *d = home_runtime.data;
        bool dirty = home_runtime.dirty, setup = home_runtime.setup;
        bool force = home_runtime.force_show;
        bool leaving_setup = drew_setup && !setup; /* the first real picture after the card */
        int screen = home_runtime.pending_screen;
        uint64_t request_id = home_runtime.request_id, manual_id = home_runtime.manual_id;
        bool manual_request = home_runtime.pending_manual;
        int current = home_runtime.displayed_screen;
        int phase = home_runtime.phase;
        int64_t manual = home_runtime.manual_until, last_switch = home_runtime.last_switch;
        bool clock_synced = home_runtime.time_valid; /* SNTP has set the clock (sources task) */
        /* The setup card lives exactly as long as the window it describes: once the access point
         * and the code are gone, the card would be an instruction to nowhere. */
        if (home_runtime.setup && mono >= home_runtime.pair_until) {
            home_runtime.setup = false;
            home_runtime.dirty = true;
            home_runtime.request_id++;
        }
        /* The "emini" card holds the display for its window, then the screen comes back. */
        bool info_open = home_runtime.info_until > mono;
        if (!info_open && home_runtime.info_until) {
            home_runtime.info_until = 0;
            home_runtime.dirty = true;
            home_runtime.request_id++;
            dirty = true;
        }
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
            /* Counters for the "emini" card: minutes awake, the first start we know of, and one
             * battery reading a day. Written to NVS every 30 minutes, so the flash sees little. */
            home_counters_t *n = &home_runtime.counters;
            n->awake_minutes++;
            if (!n->first_start)
                n->first_start = now;
            int64_t midnight = now - (now % 86400);
            if (n->day_stamp != midnight) {
                int days = n->day_stamp ? (int)((midnight - n->day_stamp) / 86400) : HOME_BATTERY_DAYS;
                for (int k = HOME_BATTERY_DAYS - 1; k >= 0; --k)
                    n->battery_day[k] = k >= days ? n->battery_day[k - days] : -1;
                n->day_stamp = midnight;
            }
            if (home_runtime.battery.percent_estimate >= 0)
                n->battery_day[0] = (int8_t)home_runtime.battery.percent_estimate;
            bool save = n->awake_minutes % 30 == 0;
            home_counters_t copy = *n;
            home_unlock();
            if (save)
                home_store_stats(&copy);
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
            (!quiet || setup || leaving_setup || manual_request || mono < manual ||
             !home_runtime.frame_valid)) {
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
                home_render_setup(ssid, pass, code, address, home_language(c->locale), work);
            else if (info_open) {
                home_stats_t stats;
                home_lock();
                home_stats_snapshot(&stats, valid ? now : 0);
                home_unlock();
                home_render_info(c, &stats, valid ? now : 0, work);
            } else
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
            if (have_hash && !memcmp(hash, last_hash, 32) && !force) {
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
            home_runtime.force_show = false; /* answered */
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
                home_runtime.counters.pictures++;
                drew_setup = setup;
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
