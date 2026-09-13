#include "home_discovery.h"
#include "mdns.h"
#include "esp_app_desc.h"
#include <stdbool.h>
#include <string.h>

/* The advertised name is the public random AP suffix, not a MAC, token,
 * account, private device identifier, user-provided room name, or location. */
static char registered_hostname[10];
static bool registered;

static bool normalized_name(const char *input, char output[10])
{
    if (!input)
        return false;
    size_t length = 0;
    while (length < 10 && input[length])
        ++length;
    if (length != 9)
        return false;
    for (size_t i = 0; i < 9; ++i) {
        unsigned char c = (unsigned char)input[i];
        if (c >= 'A' && c <= 'Z')
            c = (unsigned char)(c - 'A' + 'a');
        output[i] = (char)c;
    }
    output[9] = '\0';
    if (memcmp(output, "home-", 5))
        return false;
    for (size_t i = 5; i < 9; ++i)
        if (!((output[i] >= '0' && output[i] <= '9') ||
              (output[i] >= 'a' && output[i] <= 'f')))
            return false;
    return true;
}

esp_err_t home_discovery_start(const char *hostname)
{
    char name[10];
    if (!normalized_name(hostname, name))
        return ESP_ERR_INVALID_ARG;
    if (registered)
        return strcmp(name, registered_hostname) ? ESP_ERR_INVALID_STATE : ESP_OK;

    esp_err_t result = mdns_init();
    if (result != ESP_OK)
        return result;
    result = mdns_hostname_set(name);
    if (result == ESP_OK) {
        mdns_txt_item_t txt[] = {
            {"model", "NOTE4C"},
            {"version", esp_app_get_description()->version},
        };
        /* NULL instance uses the hostname; two units with different public
         * suffixes do not deliberately share one service-instance name. */
        result = mdns_service_add(NULL, "_http", "_tcp", 80, txt,
                                  sizeof(txt) / sizeof(txt[0]));
    }
    if (result != ESP_OK) {
        /* Only free an instance this startup call successfully initialized.
         * No global DNS resolver, captive portal, network reset or retry loop. */
        mdns_free();
        return result;
    }
    memcpy(registered_hostname, name, sizeof(name));
    registered = true;
    return ESP_OK;
}
