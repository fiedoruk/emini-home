/* Trusted, physically attached USB maintenance transport. No network route.
 * Explicit USB pair opens the same five-minute window as the physical button.
 * Credentials are accepted only in that physical pairing window.
 * Read directly from the default nonblocking USB VFS: no terminal echo. */
#include "home_runtime.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
        int n = read(STDIN_FILENO, &ch, 1);
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
        } else {
            if ((used || discard) && esp_timer_get_time() - last > INT64_C(5000000)) {
                memset(line, 0, sizeof(line));
                used = 0;
                discard = true;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
esp_err_t home_usb_start(void)
{
    return xTaskCreate(task, "home_usb", 4096, NULL, 3, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
