#include "home_board.h"

#include "driver/gpio.h"
#include "esp_err.h"

static void output(gpio_num_t pin, int value)
{
    ESP_ERROR_CHECK(gpio_hold_dis(pin));
    ESP_ERROR_CHECK(gpio_set_level(pin, value));
    gpio_config_t config = {.pin_bit_mask = 1ULL << pin, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(gpio_set_level(pin, value));
    ESP_ERROR_CHECK(gpio_hold_en(pin));
}

void home_board_init(void)
{
    output(GPIO_NUM_17, 1);
    output(GPIO_NUM_21, 0);
    output(GPIO_NUM_42, 0);
    output(GPIO_NUM_46, 0);
    gpio_config_t buttons = {
        .pin_bit_mask = (1ULL << GPIO_NUM_39) | (1ULL << GPIO_NUM_18) | (1ULL << GPIO_NUM_0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&buttons));
}

int home_board_keys(void)
{
    return (!gpio_get_level(GPIO_NUM_39) ? 1 : 0) | (!gpio_get_level(GPIO_NUM_18) ? 2 : 0) |
           (!gpio_get_level(GPIO_NUM_0) ? 4 : 0);
}
