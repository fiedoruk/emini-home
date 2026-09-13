/*
 * NOTE4C-only adapter. Pin map and four-colour command sequence adapted from
 * LazyYoun/youn-ink-fourcolor-firmware at
 * 51812e4ab3fa80ba7a5a5a274635ca2cf3901a25, config.h and custom_lcd_display.cc.
 * See docs/HARDWARE.md for the upstream commit and Home's changes.
 *
 * MIT License (upstream notices retained for the adapted portions)
 * Copyright (c) 2026 macheng2017
 * Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd.
 * Copyright (c) 2025 Project Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "home_panel.h"

#include <stdbool.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PANEL_RAIL GPIO_NUM_6
#define PANEL_BUSY GPIO_NUM_8
#define PANEL_RST GPIO_NUM_9
#define PANEL_DC GPIO_NUM_10
#define PANEL_CS GPIO_NUM_11
#define PANEL_SCK GPIO_NUM_12
#define PANEL_MOSI GPIO_NUM_13
#define PANEL_SPI SPI3_HOST
#define BUSY_TIMEOUT_US INT64_C(120000000)
#define REFRESH_ASSERT_TIMEOUT_US INT64_C(1000000)
#define SPI_TIMEOUT_MS 1000U

typedef enum {
    PANEL_NEW,
    PANEL_OFF,
    PANEL_ACTIVE,
    PANEL_REFRESHED,
    PANEL_FAULT
} panel_state_t;

static const char *TAG = "home_panel";
static panel_state_t panel_state = PANEL_NEW;
static spi_device_handle_t panel_spi;
static TaskHandle_t owner_task;
static portMUX_TYPE owner_guard = portMUX_INITIALIZER_UNLOCKED;
/* A timed-out transaction may still belong to IDF. Static storage must remain
 * alive and unchanged after FAULT; no automatic bus teardown or reuse. */
static DMA_ATTR uint8_t transfer_buffer[HOME_PANEL_ROW_BYTES];
static spi_transaction_t transaction;

static TickType_t ticks_at_least_one(uint32_t milliseconds)
{
    TickType_t ticks = pdMS_TO_TICKS(milliseconds);
    return ticks > 0 ? ticks : 1;
}

static void delay_ms(uint32_t milliseconds)
{
    /* Round upward and allow for the current partial tick, preserving minimum
     * vendor delays even when called just before the next scheduler tick. */
    TickType_t ticks = (milliseconds + portTICK_PERIOD_MS - 1U) / portTICK_PERIOD_MS;
    vTaskDelay(ticks + 1U);
}

static esp_err_t check_owner(bool claim)
{
    if (xPortInIsrContext()) {
        return ESP_ERR_INVALID_STATE;
    }
    TaskHandle_t current = xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&owner_guard);
    if (claim && owner_task == NULL) {
        owner_task = current;
    }
    bool matches = owner_task == current;
    portEXIT_CRITICAL(&owner_guard);
    return matches ? ESP_OK : ESP_ERR_INVALID_STATE;
}

static esp_err_t fault(esp_err_t error, const char *stage)
{
    panel_state = PANEL_FAULT;
    ESP_LOGE(TAG, "FAULT stage=%s error=%s; recovery decision required, no retry",
             stage, esp_err_to_name(error));
    return error;
}

#define PANEL_TRY(call, stage) do { \
    esp_err_t panel_error_ = (call); \
    if (panel_error_ != ESP_OK) { return fault(panel_error_, (stage)); } \
} while (0)

static esp_err_t set_rail(int level)
{
    PANEL_TRY(gpio_hold_dis(PANEL_RAIL), "rail hold disable");
    PANEL_TRY(gpio_set_level(PANEL_RAIL, level), "rail level");
    PANEL_TRY(gpio_hold_en(PANEL_RAIL), "rail hold enable");
    return ESP_OK;
}

static esp_err_t wait_idle(const char *stage)
{
    int64_t start = esp_timer_get_time();
    int64_t last_log = start;
    while (gpio_get_level(PANEL_BUSY) == 0) {
        int64_t now = esp_timer_get_time();
        if (now - start >= BUSY_TIMEOUT_US) {
            return fault(ESP_ERR_TIMEOUT, stage);
        }
        if (now - last_log >= INT64_C(5000000)) {
            ESP_LOGI(TAG, "BUSY stage=%s elapsed_ms=%ld", stage,
                     (long)((now - start) / 1000));
            last_log = now;
        }
        delay_ms(50);
    }
    return ESP_OK;
}

static esp_err_t wait_refresh_asserted(void)
{
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(PANEL_BUSY) != 0) {
        if (esp_timer_get_time() - start >= REFRESH_ASSERT_TIMEOUT_US) {
            return fault(ESP_ERR_TIMEOUT, "refresh did not assert BUSY");
        }
        vTaskDelay(1);
    }
    return ESP_OK;
}

static esp_err_t transfer(int data_mode, const uint8_t *bytes, size_t len)
{
    if (bytes == NULL || len == 0 || len > sizeof(transfer_buffer)) {
        return fault(ESP_ERR_INVALID_ARG, "internal transfer bounds");
    }
    memcpy(transfer_buffer, bytes, len);
    memset(&transaction, 0, sizeof(transaction));
    transaction.length = len * 8U;
    transaction.tx_buffer = transfer_buffer;
    PANEL_TRY(gpio_set_level(PANEL_DC, data_mode), "SPI DC");
    PANEL_TRY(gpio_set_level(PANEL_CS, 0), "SPI CS assert");
    PANEL_TRY(spi_device_queue_trans(panel_spi, &transaction,
                                    ticks_at_least_one(SPI_TIMEOUT_MS)), "SPI queue");
    spi_transaction_t *completed = NULL;
    PANEL_TRY(spi_device_get_trans_result(panel_spi, &completed,
                                         ticks_at_least_one(SPI_TIMEOUT_MS)), "SPI completion");
    if (completed != &transaction) {
        return fault(ESP_ERR_INVALID_STATE, "unexpected SPI transaction");
    }
    PANEL_TRY(gpio_set_level(PANEL_CS, 1), "SPI CS release");
    return ESP_OK;
}

static esp_err_t command(uint8_t byte)
{
    return transfer(0, &byte, 1);
}

static esp_err_t data(uint8_t byte)
{
    return transfer(1, &byte, 1);
}

esp_err_t home_panel_init(void)
{
    if (check_owner(true) != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    if (panel_state == PANEL_OFF) {
        return ESP_OK;
    }
    if (panel_state != PANEL_NEW) {
        return ESP_ERR_INVALID_STATE;
    }
    gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << PANEL_RST) | (1ULL << PANEL_DC) |
                        (1ULL << PANEL_CS) | (1ULL << PANEL_RAIL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    PANEL_TRY(gpio_config(&output_config), "GPIO outputs");
    PANEL_TRY(set_rail(0), "initial rail off");
    PANEL_TRY(gpio_set_level(PANEL_RST, 1), "initial reset release");
    PANEL_TRY(gpio_set_level(PANEL_CS, 1), "initial chip deselect");
    PANEL_TRY(gpio_set_level(PANEL_DC, 1), "initial data mode");
    gpio_config_t busy_config = {
        .pin_bit_mask = 1ULL << PANEL_BUSY,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    PANEL_TRY(gpio_config(&busy_config), "GPIO BUSY");
    spi_bus_config_t bus_config = {
        .mosi_io_num = PANEL_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PANEL_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = HOME_PANEL_ROW_BYTES,
    };
    spi_device_interface_config_t device_config = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
    };
    PANEL_TRY(spi_bus_initialize(PANEL_SPI, &bus_config, SPI_DMA_CH_AUTO), "SPI bus init");
    PANEL_TRY(spi_bus_add_device(PANEL_SPI, &device_config, &panel_spi), "SPI device init");
    panel_state = PANEL_OFF;
    ESP_LOGI(TAG, "NOTE4C transport initialized: 400x300 2bpp SPI3 40MHz, rail off");
    return ESP_OK;
}

esp_err_t home_panel_power_off(void)
{
    if (check_owner(false) != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    if (panel_state == PANEL_OFF) {
        return ESP_OK;
    }
    if (panel_state != PANEL_REFRESHED) {
        return ESP_ERR_INVALID_STATE;
    }
    PANEL_TRY(command(0x02), "power-off command");
    PANEL_TRY(data(0x00), "power-off data");
    PANEL_TRY(wait_idle("power off"), "power-off BUSY");
    delay_ms(20);
    PANEL_TRY(command(0x07), "deep-sleep command");
    PANEL_TRY(data(0xA5), "deep-sleep data");
    PANEL_TRY(set_rail(0), "normal rail off");
    panel_state = PANEL_OFF;
    return ESP_OK;
}

esp_err_t home_panel_show(const uint8_t *frame, size_t len)
{
    if (frame == NULL || len != HOME_PANEL_FRAME_BYTES) {
        return ESP_ERR_INVALID_ARG;
    }
    if (check_owner(false) != ESP_OK || panel_state != PANEL_OFF) {
        return ESP_ERR_INVALID_STATE;
    }
    int64_t start = esp_timer_get_time();
    panel_state = PANEL_ACTIVE;
    PANEL_TRY(set_rail(1), "rail on");
    delay_ms(10);
    PANEL_TRY(gpio_set_level(PANEL_RST, 1), "reset high");
    delay_ms(10);
    PANEL_TRY(gpio_set_level(PANEL_RST, 0), "reset low");
    delay_ms(20);
    PANEL_TRY(gpio_set_level(PANEL_RST, 1), "reset release");
    delay_ms(10);
    PANEL_TRY(wait_idle("reset"), "reset BUSY");
    /* Four-colour source path uses OTP and returns here; deliberately omit
     * the monochrome 00 2F 2E and temperature/LUT adaptation commands. */
    PANEL_TRY(command(0xE9), "OTP command");
    PANEL_TRY(data(0x01), "OTP data");
    PANEL_TRY(command(0x10), "frame command");
    PANEL_TRY(wait_idle("frame write"), "frame BUSY");
    for (size_t row = 0; row < HOME_PANEL_HEIGHT; ++row) {
        PANEL_TRY(transfer(1, frame + row * HOME_PANEL_ROW_BYTES,
                           HOME_PANEL_ROW_BYTES), "frame row");
        if ((row % 16U) == 15U) {
            vTaskDelay(1);
        }
    }
    PANEL_TRY(command(0x04), "power-on command");
    PANEL_TRY(wait_idle("power on"), "power-on BUSY");
    delay_ms(10);
    PANEL_TRY(command(0x12), "refresh command");
    PANEL_TRY(data(0x00), "refresh data");
    /* Home diagnostic guard: a pulled-up/unconnected BUSY is not success. */
    PANEL_TRY(wait_refresh_asserted(), "refresh BUSY assertion");
    delay_ms(10);
    PANEL_TRY(wait_idle("refresh"), "refresh BUSY completion");
    panel_state = PANEL_REFRESHED;
    PANEL_TRY(home_panel_power_off(), "normal power-down");
    ESP_LOGI(TAG, "Refresh cycle completed; rail off; elapsed_ms=%ld",
             (long)((esp_timer_get_time() - start) / 1000));
    return ESP_OK;
}
