/* NM-EPD-420-4C: GPIO3/ADC1_CH2 through a 1:2 divider, enabled by GPIO43. */
#include "home_battery.h"
#include "home_runtime.h"
#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

void home_battery_task(void *unused)
{
    (void)unused;
    adc_oneshot_unit_handle_t adc = NULL;
    adc_cali_handle_t calibration = NULL;
    adc_oneshot_unit_init_cfg_t unit = {.unit_id = ADC_UNIT_1};
    adc_oneshot_chan_cfg_t channel = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12};
    adc_cali_curve_fitting_config_t cal = {
        .unit_id = ADC_UNIT_1, .chan = ADC_CHANNEL_2,
        .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12};
    gpio_config_t enable = {.pin_bit_mask = 1ULL << GPIO_NUM_43, .mode = GPIO_MODE_OUTPUT};
    if (gpio_config(&enable) != ESP_OK || gpio_set_level(GPIO_NUM_43, 1) != ESP_OK ||
        adc_oneshot_new_unit(&unit, &adc) != ESP_OK ||
        adc_oneshot_config_channel(adc, ADC_CHANNEL_2, &channel) != ESP_OK ||
        adc_cali_create_scheme_curve_fitting(&cal, &calibration) != ESP_OK) {
        if (adc) adc_oneshot_del_unit(adc);
        ESP_LOGW("home_battery", "Calibrated battery reading unavailable");
        vTaskDelete(NULL);
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(80));
    int64_t next_voltage = 0;
    home_battery_t sample = {.percent_estimate = -1};
    while (true) {
        sample.charge_valid = true;
        sample.charging = false;
        sample.full = false;
        int64_t now = esp_timer_get_time();
        if (now >= next_voltage) {
            int sum = 0, count = 0;
            for (int index = 0; index < 10; index++) {
                int raw, mv;
                if (adc_oneshot_read(adc, ADC_CHANNEL_2, &raw) == ESP_OK &&
                    adc_cali_raw_to_voltage(calibration, raw, &mv) == ESP_OK) {
                    sum += mv * 2;
                    count++;
                }
            }
            sample.millivolts = count == 10 ? sum / 10 : 0;
            sample.valid = count == 10 && sample.millivolts >= 2800 && sample.millivolts <= 4350;
            sample.measured_at = now / 1000000;
            next_voltage = now + INT64_C(30000000);
        }
        sample.percent_estimate = -1;
        if (sample.valid) {
            int voltage = sample.millivolts;
            int percent = (-voltage * voltage + 9016 * voltage - 19189000) / 10000;
            sample.percent_estimate = percent < 0 ? 0 : percent > 100 ? 100 : percent;
        }
        home_lock();
        home_runtime.battery = sample;
        home_unlock();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}