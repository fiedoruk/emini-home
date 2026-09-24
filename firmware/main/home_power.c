#include "home_power.h"
#include "home_runtime.h"
#include "esp_pm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/usb_serial_jtag.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include <stdio.h>

static const char *TAG = "home_power";
static esp_pm_lock_handle_t no_sleep, cpu_max;
static bool holding = true;      /* start like 0.5.2 until we have seen the cable state */
static int64_t stable_since;
static unsigned last_reasons;
static int64_t held_us, free_us, accounted_at;
static bool enabled;

#define SETTLE_US INT64_C(3000000)

esp_err_t home_power_init(void)
{
    /* 160/80 MHz: 40 MHz turns the PLL off and would be a separate, measured step. Light sleep is on,
     * but the locks below keep it away from the cable, charging, the setup window and the service mode. */
    esp_pm_config_t pm = {.max_freq_mhz = 160, .min_freq_mhz = 80, .light_sleep_enable = true};
    esp_err_t e = esp_pm_configure(&pm);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "Power management configuration failed: %s", esp_err_to_name(e));
        return e;
    }
    e = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "home_tether", &no_sleep);
    if (e == ESP_OK)
        e = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "home_tether_cpu", &cpu_max);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "Power management locks unavailable: %s", esp_err_to_name(e));
        return e;
    }
    /* On the ESP32-S3 CONFIG_PM_SLP_DISABLE_GPIO is FORCED: ESP_SLEEP_GPIO_RESET_WORKAROUND
     * defaults to y and does `select PM_SLP_DISABLE_GPIO if FREERTOS_USE_TICKLESS_IDLE`. A
     * "# ... is not set" line in sdkconfig.defaults is silently ignored - the Kconfig select
     * wins (check the generated sdkconfig, not the defaults). The workaround has a reason:
     * without it, an input-only pin can reset the chip on an electrostatic discharge during
     * sleep. Instead of fighting the option we release, one pin at a time, only the pins that
     * MUST behave in sleep exactly as they do awake:
     *  - the three buttons, or they would not wake the chip,
     *  - the power latch (17) and the panel rail (6), since the device lives or dies by them,
     *  - the front LED (3), so it stays off. */
    static const gpio_num_t keep_awake[] = {
        GPIO_NUM_39, GPIO_NUM_18, GPIO_NUM_0, /* buttons: otherwise they would not wake the chip */
        GPIO_NUM_17,                          /* power latch: the device lives or dies by it */
        GPIO_NUM_3,                           /* front status LED: keep it off during light sleep */
        GPIO_NUM_6,                           /* panel rail */
        /* The panel control lines. A paper refresh takes 25 s and for most of it the loop only
         * waits for BUSY in vTaskDelay - so the chip MAY fall asleep in that time. If chip
         * select (CS) were left floating, the paper controller could see a spurious select in
         * the middle of its own cycle. Reset and the data/command line, for the same reason.
         * Released from the sleep configuration like the buttons. */
        GPIO_NUM_11, GPIO_NUM_10, GPIO_NUM_9};
    for (size_t i = 0; i < sizeof keep_awake / sizeof keep_awake[0]; ++i) {
        esp_err_t g = gpio_sleep_sel_dis(keep_awake[i]);
        if (g != ESP_OK)
            ESP_LOGW(TAG, "Pin %d keeps the sleep isolation: %s", (int)keep_awake[i],
                     esp_err_to_name(g));
    }
    /* Waking from light sleep needs a LEVEL, not an edge: the GPIO peripheral is not clocked
     * while the chip sleeps, so an edge simply never happens. The buttons are active low, so
     * "pressed" is the level we wake on; the edge interrupt registered in home_loop_begin()
     * stays as it is and does the work while the chip is awake. */
    static const gpio_num_t buttons[] = {GPIO_NUM_39, GPIO_NUM_18, GPIO_NUM_0};
    for (size_t i = 0; i < sizeof buttons / sizeof buttons[0]; ++i) {
        esp_err_t g = gpio_wakeup_enable(buttons[i], GPIO_INTR_LOW_LEVEL);
        if (g != ESP_OK)
            ESP_LOGW(TAG, "Pin %d will not wake the chip: %s", (int)buttons[i],
                     esp_err_to_name(g));
    }
    esp_err_t w = esp_sleep_enable_gpio_wakeup();
    if (w != ESP_OK)
        ESP_LOGW(TAG, "GPIO wake-up unavailable: %s", esp_err_to_name(w));
    esp_pm_lock_acquire(no_sleep);
    esp_pm_lock_acquire(cpu_max);
    holding = true;
    stable_since = 0;
    enabled = true;
    accounted_at = esp_timer_get_time();
    ESP_LOGI(TAG, "Power management on: 160/80 MHz, light sleep between events, locks held");
    return ESP_OK;
}

/* Bits for the log only - so one line shows WHAT keeps the chip awake. */
enum { WHY_USB = 1, WHY_CHARGER = 2, WHY_SETUP = 4, WHY_MAINTENANCE = 8, WHY_UNPAIRED = 16 };

bool home_power_tick(void)
{
    if (!no_sleep)
        return true;
    int64_t now = esp_timer_get_time();
    /* How long the chip was held awake by us, and how long it was free to sleep. Counted here
     * because this is the only place that knows the lock state; shared through /api/status. */
    if (accounted_at) {
        int64_t slice = now - accounted_at;
        if (slice > 0) {
            if (holding) held_us += slice;
            else free_us += slice;
        }
    }
    accounted_at = now;
#ifdef HOME_PM_PROFILE
    /* Diagnostic build only (idf.py -DHOME_PM_PROFILE=1). Without this there is no way to SEE
     * whether the chip really sleeps and who is holding it awake - the panel looks identical
     * either way. Never in a release: it prints to the console every minute and, below, it
     * lets the device sleep even on a cable. */
    static int64_t last_dump;
    if (now - last_dump >= INT64_C(60000000)) {
        last_dump = now;
        printf("HOME_PM_LOCKS uptime_s=%lld held=%d reasons=0x%02x\n",
               (long long)(now / 1000000), holding ? 1 : 0, last_reasons);
        esp_pm_dump_locks(stdout);
        /* Pins that can draw current but that the firmware never configures.
         * GPIO3 is the status LED on this board - the maker drives it explicitly, we never do.
         * If it is lit, that is a dozen mA or more, more than our whole budget. */
        printf("HOME_PM_PINS led3=%d nfc21=%d audio42=%d amp46=%d rail6=%d latch17=%d\n",
               gpio_get_level(GPIO_NUM_3), gpio_get_level(GPIO_NUM_21),
               gpio_get_level(GPIO_NUM_42), gpio_get_level(GPIO_NUM_46),
               gpio_get_level(GPIO_NUM_6), gpio_get_level(GPIO_NUM_17));
        fflush(stdout);
    }
#endif
    unsigned why = 0;
#ifndef HOME_PM_PROFILE /* the diagnostic build must be able to sleep with the cable attached */
    if (usb_serial_jtag_is_connected())
        why |= WHY_USB;
#endif
    home_lock();
    const home_battery_t *b = &home_runtime.battery;
    if (b->charge_valid && (b->charging || b->full))
        why |= WHY_CHARGER;
    if (home_runtime.setup || home_runtime.pair_until > now)
        why |= WHY_SETUP;
    if (home_runtime.maintenance)
        why |= WHY_MAINTENANCE;
    bool paired = false;
    for (int i = 0; i < 4; i++)
        paired |= home_runtime.secrets.token_used[i] != 0;
    if (!paired)
        why |= WHY_UNPAIRED; /* nobody has the panel yet: do not hide */
    home_unlock();

    bool want = why != 0;
    if (want == holding) {
        stable_since = 0;
        last_reasons = why;
        return holding;
    }
    /* Hysteresis: the charger pin and the cable detection can flicker. */
    if (!stable_since)
        stable_since = now;
    if (now - stable_since < SETTLE_US)
        return holding;
    stable_since = 0;
    if (want) {
        esp_pm_lock_acquire(no_sleep);
        esp_pm_lock_acquire(cpu_max);
    } else {
        esp_pm_lock_release(cpu_max);
        esp_pm_lock_release(no_sleep);
    }
    holding = want;
    ESP_LOGI(TAG, "POWER locks=%s reasons=0x%02x (was 0x%02x)", want ? "held" : "released", why,
             last_reasons);
    last_reasons = why;
    return holding;
}

void home_power_snapshot(home_power_state_t *out)
{
    if (!out)
        return;
    out->enabled = enabled;
    out->locks_held = holding;
    out->reasons = last_reasons;
    out->held_s = held_us / 1000000;
    out->free_s = free_us / 1000000;
}
