/* NM-EPD-420-4C GDEY0420F51/HX8717 adapter.
 * Command sequence adapted from RockBase-iot/NM-EPD-420 and GxEPD2's
 * GDEY0420F51 driver; see docs/HARDWARE.md for source details. */
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

#define PANEL_BUSY GPIO_NUM_6
#define PANEL_RST GPIO_NUM_5
#define PANEL_DC GPIO_NUM_4
#define PANEL_CS GPIO_NUM_46
#define PANEL_SCK GPIO_NUM_2
#define PANEL_MOSI GPIO_NUM_1
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
static DMA_ATTR uint8_t transfer_buffer[HOME_PANEL_ROW_BYTES];
static spi_transaction_t transaction;
static void (*idle_hook)(void);

static TickType_t ticks_at_least_one(uint32_t milliseconds)
{
    TickType_t ticks = pdMS_TO_TICKS(milliseconds);
    return ticks > 0 ? ticks : 1;
}

static void delay_ms(uint32_t milliseconds)
{
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

void home_panel_set_idle_hook(void (*hook)(void))
{
    idle_hook = hook;
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
        if (idle_hook) {
            idle_hook();
        }
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

static esp_err_t command_data(uint8_t command_byte, const uint8_t *bytes, size_t len,
                              const char *stage)
{
    PANEL_TRY(command(command_byte), stage);
    if (len > 0) {
        PANEL_TRY(transfer(1, bytes, len), stage);
    }
    return ESP_OK;
}

static esp_err_t initialize_controller(void)
{
    static const uint8_t panel_setting[] = {0x0F, 0x09};
    static const uint8_t power_setting[] = {0x07, 0x00, 0x22, 0x78, 0x0A, 0x22};
    static const uint8_t power_off[] = {0x10, 0x54, 0x44};
    static const uint8_t booster[] = {0x26, 0x26, 0x26};
    static const uint8_t pll[] = {0x02};
    static const uint8_t temperature[] = {0x00};
    static const uint8_t vcom_interval[] = {0x37};
    static const uint8_t timing[] = {0x02, 0x02};
    static const uint8_t resolution[] = {0x01, 0x90, 0x01, 0x2C};
    static const uint8_t unknown_65[] = {0x00, 0x00, 0x00, 0x00};
    static const uint8_t vcom[] = {0xAD};
    static const uint8_t unknown_e7[] = {0x1C};
    static const uint8_t power_save[] = {0x22};
    static const uint8_t unknown_e0[] = {0x00};
    static const uint8_t otp[] = {0x01};

    PANEL_TRY(command_data(0x00, panel_setting, sizeof(panel_setting), "panel setting"),
              "panel setting");
    PANEL_TRY(command_data(0x01, power_setting, sizeof(power_setting), "power setting"),
              "power setting");
    PANEL_TRY(command_data(0x03, power_off, sizeof(power_off), "power-off timing"),
              "power-off timing");
    PANEL_TRY(command_data(0x06, booster, sizeof(booster), "booster setting"),
              "booster setting");
    PANEL_TRY(command_data(0x30, pll, sizeof(pll), "PLL setting"), "PLL setting");
    PANEL_TRY(command_data(0x41, temperature, sizeof(temperature), "temperature setting"),
              "temperature setting");
    PANEL_TRY(command_data(0x50, vcom_interval, sizeof(vcom_interval), "VCOM interval"),
              "VCOM interval");
    PANEL_TRY(command_data(0x60, timing, sizeof(timing), "timing setting"), "timing setting");
    PANEL_TRY(command_data(0x61, resolution, sizeof(resolution), "resolution setting"),
              "resolution setting");
    PANEL_TRY(command_data(0x65, unknown_65, sizeof(unknown_65), "setting 65"), "setting 65");
    PANEL_TRY(command_data(0x82, vcom, sizeof(vcom), "VCOM setting"), "VCOM setting");
    PANEL_TRY(command_data(0xE7, unknown_e7, sizeof(unknown_e7), "setting E7"), "setting E7");
    PANEL_TRY(command_data(0xE3, power_save, sizeof(power_save), "power-save setting"),
              "power-save setting");
    PANEL_TRY(command_data(0xE0, unknown_e0, sizeof(unknown_e0), "setting E0"), "setting E0");
    PANEL_TRY(command_data(0xE9, otp, sizeof(otp), "OTP setting"), "OTP setting");
    PANEL_TRY(command(0x04), "power-on command");
    PANEL_TRY(wait_idle("power on"), "power-on BUSY");
    return ESP_OK;
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
        .pin_bit_mask = (1ULL << PANEL_RST) | (1ULL << PANEL_DC) | (1ULL << PANEL_CS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    PANEL_TRY(gpio_config(&output_config), "GPIO outputs");
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
    ESP_LOGI(TAG, "NM-EPD-420-4C transport initialized: 400x300 2bpp SPI3 40MHz");
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
    PANEL_TRY(command_data(0x02, (const uint8_t[]){0x00}, 1, "power-off command"),
              "power-off command");
    PANEL_TRY(wait_idle("power off"), "power-off BUSY");
    delay_ms(20);
    PANEL_TRY(command_data(0x07, (const uint8_t[]){0xA5}, 1, "deep-sleep command"),
              "deep-sleep command");
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
    PANEL_TRY(gpio_set_level(PANEL_RST, 1), "reset high");
    delay_ms(20);
    PANEL_TRY(gpio_set_level(PANEL_RST, 0), "reset low");
    delay_ms(2);
    PANEL_TRY(gpio_set_level(PANEL_RST, 1), "reset release");
    delay_ms(2);
    PANEL_TRY(wait_idle("reset"), "reset BUSY");
    PANEL_TRY(initialize_controller(), "controller initialization");
    PANEL_TRY(command(0x10), "frame command");
    for (size_t row = 0; row < HOME_PANEL_HEIGHT; ++row) {
        PANEL_TRY(transfer(1, frame + row * HOME_PANEL_ROW_BYTES,
                           HOME_PANEL_ROW_BYTES), "frame row");
        if ((row % 16U) == 15U) {
            vTaskDelay(1);
        }
    }
    static const uint8_t refresh_vcom[] = {0x37};
    PANEL_TRY(command_data(0x50, refresh_vcom, sizeof(refresh_vcom), "refresh VCOM interval"),
              "refresh VCOM interval");
    PANEL_TRY(command_data(0x12, (const uint8_t[]){0x00}, 1, "refresh command"),
              "refresh command");
    PANEL_TRY(wait_refresh_asserted(), "refresh BUSY assertion");
    delay_ms(10);
    PANEL_TRY(wait_idle("refresh"), "refresh BUSY completion");
    panel_state = PANEL_REFRESHED;
    PANEL_TRY(home_panel_power_off(), "normal power-down");
    ESP_LOGI(TAG, "Refresh cycle completed; elapsed_ms=%ld",
             (long)((esp_timer_get_time() - start) / 1000));
    return ESP_OK;
}