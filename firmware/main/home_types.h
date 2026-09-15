#ifndef HOME_TYPES_H
#define HOME_TYPES_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define HOME_SCHEMA 1
#define HOME_SCREEN_COUNT 3
#define HOME_FRAME_BYTES 30000
#define HOME_NOTE_BYTES 241
#define HOME_FEED_URL_BYTES 513
typedef enum { HOME_WEATHER=0, HOME_FEED=1, HOME_NOTE=2 } home_screen_t;
typedef enum { HOME_FIXED=0, HOME_DAY=1, HOME_ROTATE=2 } home_mode_t;
/* HOME_CYCLE is stored in the config only; drawing always gets one of the first three. */
typedef enum { HOME_PRINT=0, HOME_RHYTHM=1, HOME_ATLAS=2, HOME_CYCLE=3 } home_style_t;
typedef enum { HOME_EMPTY=0, HOME_READY=1, HOME_STALE=2, HOME_ERROR=3 } home_source_state_t;
typedef struct {
    uint32_t revision;
    char name[49];
    char locale[3];
    char units[2];
    char timezone[49];
    char location[65];
    double latitude, longitude;
    bool location_ready;
    char note[HOME_NOTE_BYTES];
    char feed_url[HOME_FEED_URL_BYTES];
    bool enabled[3];
    uint8_t order[3];
    uint8_t style[3];
    uint8_t texture; /* cell size 1,2,4 */
    uint8_t intensity; /* 0,1,2 */
    bool large_text, clock24;
    home_mode_t mode;
    uint8_t fixed_screen;
    uint16_t interval_min, pause_min;
    uint16_t cycle_min; /* "In turn": minutes per composition while a screen stays */
    uint8_t ok_action;  /* short OK/BOOT: 0 language, 1 refresh, 2 hold, 3 setup window */
    bool quiet_enabled;
    uint16_t quiet_start, quiet_end;
    uint8_t weekdays; /* Monday bit0 */
    uint16_t day_minute[3];
    uint8_t day_screen[3];
} home_config_t;

typedef struct {
    bool valid;
    bool no_store; /* provider forbids persistent response cache */
    home_source_state_t state;
    int64_t issued_at, fetched_at, checked_at, expires_at, next_fetch;
    char error[97];
    char etag[129];
    char last_modified[65];
} home_source_meta_t;
typedef struct {
    home_source_meta_t meta;
    int64_t forecast_at; /* validity time of temperature/hourly[0], separate from model issue */
    double temperature, low, high, precipitation, wind_speed, cloud_cover;
    char symbol[49];
    double hourly_temperature[12], hourly_rain[12];
    uint8_t hourly_count;
} home_weather_t;
typedef struct {
    home_source_meta_t meta;
    char title[257];
    char source[97];
    char url[HOME_FEED_URL_BYTES];
    int64_t published_at;
} home_feed_t;
typedef struct { home_weather_t weather; home_feed_t feed; } home_data_t;

/* Pure C API; packed B0/W1/Y2/R3, four pixels MSB first. */
void home_render(const home_config_t *config, const home_data_t *data,
                 home_screen_t screen, int64_t now, uint8_t frame[HOME_FRAME_BYTES]);
void home_render_setup(const char *ssid, const char *password, const char *code,
                       const char *address, bool pl, uint8_t frame[HOME_FRAME_BYTES]);
void home_render_status(const char *title, const char *body, bool pl,
                       uint8_t frame[HOME_FRAME_BYTES]);
#endif
