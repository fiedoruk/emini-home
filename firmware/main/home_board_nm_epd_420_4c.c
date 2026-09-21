#include "home_board.h"

#include "driver/gpio.h"
#include "esp_err.h"

void home_board_init(void)
{
    gpio_config_t buttons = {
        .pin_bit_mask = (1ULL << GPIO_NUM_45) | (1ULL << GPIO_NUM_0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&buttons));
}

int home_board_keys(void)
{
    return (!gpio_get_level(GPIO_NUM_45) ? 2 : 0) | (!gpio_get_level(GPIO_NUM_0) ? 4 : 0);
}