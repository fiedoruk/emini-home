/* Trusted, physically attached USB maintenance transport. No network route.
 * Explicit USB pair opens the same five-minute window as the physical button.
 * Credentials are accepted only in that physical pairing window.
 * Reads block on the USB-Serial/JTAG driver. Until 0.5.2 this task woke a hundred times a
 * second to ask whether a byte had arrived - on a device whose maintenance protocol is used a
 * few times a year - and that alone kept the chip from ever being idle long enough to sleep. */
#include "home_runtime.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "freertos/task.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static bool driver_ready;
static bool fields(cJSON *j, bool wifi)
{
    if (!cJSON_IsObject(j))
        return false;
    for (cJSON *v = j->child; v; v = v->next) {
        if (strcmp(v->string, "op") &&
            (!wifi || (strcmp(v->string, "ssid") && strcmp(v->string, "password"))))
            return false;
        for (cJSON *w = v->next; w; w = w->next)
            if (!strcmp(v->string, w->string))
                return false;
    }
    return true;
}
static void scrub(cJSON *j)
{
    for (cJSON *v = j; v; v = v->next) {
        if (cJSON_IsString(v) && v->valuestring)
            memset(v->valuestring, 0, strlen(v->valuestring));
        if (v->child)
            scrub(v->child);
    }
}
static void command(char *line, size_t length)
{
    cJSON *j = NULL, *reply = cJSON_CreateObject();
    const char *message = "invalid_request";
    if (strlen(line) != length || strstr(line, "\\u0000"))
        goto done;
    j = cJSON_ParseWithLengthOpts(line, length + 1, NULL, true);
    cJSON *op = cJSON_GetObjectItemCaseSensitive(j, "op");
    if (!cJSON_IsString(op))
        goto done;
    bool wifi = !strcmp(op->valuestring, "wifi");
    if (!fields(j, wifi))
        goto done;
    home_lock();
    bool window = esp_timer_get_time() < home_runtime.pair_until;
    bool maintenance = home_runtime.maintenance;
    home_unlock();
    if (!strcmp(op->valuestring, "status")) {
        home_lock();
        cJSON_AddBoolToObject(reply, "online", home_runtime.online);
        cJSON_AddStringToObject(reply, "address", home_runtime.address);
        cJSON_AddStringToObject(reply, "hostname", home_runtime.hostname);
        cJSON_AddNumberToObject(reply, "phase", home_runtime.phase);
        cJSON_AddNumberToObject(reply, "generation", home_runtime.generation);
        cJSON_AddBoolToObject(reply, "maintenance", home_runtime.maintenance);
        cJSON_AddNumberToObject(reply, "uptime_s", esp_timer_get_time() / 1000000);
        home_unlock();
        message = "ok";
    } else if (!strcmp(op->valuestring, "pair") && !maintenance) {
        if (!window)
            home_begin_pairing();
        home_lock();
        cJSON_AddStringToObject(reply, "code", home_runtime.pair_code);
        home_unlock();
        message = "ok";
    } else if (wifi && window && !maintenance) {
        cJSON *s = cJSON_GetObjectItemCaseSensitive(j, "ssid");
        cJSON *p = cJSON_GetObjectItemCaseSensitive(j, "password");
        if (cJSON_IsString(s) && cJSON_IsString(p))
            message = home_network_credentials(s->valuestring, p->valuestring) == ESP_OK
                          ? "accepted"
                          : "settings_not_saved";
    } else if (!strcmp(op->valuestring, "scan")) {
        /* Diagnostic over the attached cable: what the radio sees (same scan the
         * setup panel uses). "scan" starts it; "networks" a few seconds later
         * returns the list, or the state of a scan still running. */
        esp_err_t e = home_network_scan_start();
        message = e == ESP_OK ? "started" : "busy";
    } else if (!strcmp(op->valuestring, "networks")) {
        cJSON *list = home_network_scan_json();
        if (list) {
            cJSON_AddItemToObject(reply, "networks", list);
            message = "ok";
        } else
            message = "no_scan";
    } else if (!strcmp(op->valuestring, "quiesce")) {
        home_lock();
        home_runtime.maintenance = true;
        home_runtime.request_id++;
        home_unlock();
        int64_t deadline = esp_timer_get_time() + INT64_C(55000000);
        bool ready = false;
        do {
            home_lock();
            ready =
                home_runtime.phase == 0 && !home_runtime.source_active && !home_runtime.api_active;
            home_unlock();
            if (!ready)
                vTaskDelay(pdMS_TO_TICKS(100));
        } while (!ready && esp_timer_get_time() < deadline);
        message = ready ? "quiesced" : "busy";
    } else if (!strcmp(op->valuestring, "resume")) {
        home_lock();
        home_runtime.maintenance = false;
        home_runtime.request_id++;
        home_unlock();
        message = "resumed";
    } else
        message = "pairing_window_required";
done:
    if (j) {
        scrub(j);
        cJSON_Delete(j);
    }
    cJSON_AddStringToObject(reply, "result", message);
    char *out = cJSON_PrintUnformatted(reply);
    if (out) {
        printf("HOME_USB:%s\n", out);
        fflush(stdout);
        memset(out, 0, strlen(out));
        free(out);
    }
    cJSON_Delete(reply);
}
static void task(void *unused)
{
    (void)unused;
    char line[384];
    size_t used = 0;
    bool discard = false;
    int64_t last = 0;
    while (true) {
        unsigned char ch;
        /* With the driver: blocks until a byte arrives or a second passes; the timeout is only
         * there so the half-typed-line rule below still fires. Without it: the pre-0.6.0 path. */
        int n;
        if (driver_ready)
            n = usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(1000));
        else {
            n = (int)read(STDIN_FILENO, &ch, 1);
            if (n != 1)
                vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (n == 1) {
            last = esp_timer_get_time();
            if (ch == '\n') {
                if (!discard && used) {
                    line[used] = 0;
                    command(line, used);
                }
                memset(line, 0, sizeof(line));
                used = 0;
                discard = false;
            } else if (!ch || used >= sizeof(line) - 1)
                discard = true;
            else if (!discard)
                line[used++] = ch;
        } else if ((used || discard) && esp_timer_get_time() - last > INT64_C(5000000)) {
            memset(line, 0, sizeof(line));
            used = 0;
            discard = true;
        }
    }
}
esp_err_t home_usb_start(void)
{
    /* The blocking read needs the driver. If it cannot be installed we fall back to the
     * non-blocking path this file used until 0.5.2 rather than refusing to start: app_main
     * wraps this call in ESP_ERROR_CHECK, so returning an error here would halt the device
     * at boot over a maintenance transport that is used a few times a year. A Home that
     * answers on Wi-Fi with a polling USB task is far better than one that does not boot. */
    usb_serial_jtag_driver_config_t usb = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    esp_err_t e = usb_serial_jtag_driver_install(&usb);
    if (e == ESP_OK || e == ESP_ERR_INVALID_STATE) {
        usb_serial_jtag_vfs_use_driver(); /* console through the same driver, so replies arrive */
        driver_ready = true;
    } else
        ESP_LOGW("home_usb", "USB driver unavailable (%s); falling back to polling",
                 esp_err_to_name(e));
    return xTaskCreate(task, "home_usb", 4096, NULL, 3, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
