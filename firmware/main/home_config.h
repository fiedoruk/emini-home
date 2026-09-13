#ifndef HOME_CONFIG_H
#define HOME_CONFIG_H
#include "home_types.h"
#include "cJSON.h"
#include <time.h>
void home_config_defaults(home_config_t *config);
bool home_config_decode(const char *json, size_t length, home_config_t *result,
                        const home_config_t *base, bool recipe, char error[128]);
cJSON *home_config_json(const home_config_t *config, bool recipe);
const char *home_timezone(const char *name);
const char *home_screen_name(int screen);
int home_screen_index(const char *name);
bool home_utf8(const char *text, size_t max_bytes, bool multiline);
bool home_is_quiet(const home_config_t *config, const struct tm *local);
int home_schedule_screen(const home_config_t *config, const struct tm *local,
                         int current, int64_t elapsed_seconds);
int home_auto_screen(const home_config_t*,const home_data_t*,const struct tm*,int,int64_t);
/* Composition to draw: the configured one, or for HOME_CYCLE the n-th of Print,
 * Rhythm, Atlas, where showing counts appearances from 1 (0 draws Print). */
int home_style_for(const home_config_t *config, int screen, uint32_t showing);
#endif
