#ifndef HOME_TYPES_H
#define HOME_TYPES_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define HOME_SCHEMA 1
/* Since 0.5.0 five screens. Settings saved by 0.4.x list three: the decoder
 * appends the missing ones at the end of the order, switched off, so the record
 * stays readable and the schema does not change. */
#define HOME_SCREEN_COUNT 5
/* Moments of the "Day rhythm", not screens: three, as in every version so far. */
#define HOME_DAY_SLOTS 3
#define HOME_FRAME_BYTES 30000
#define HOME_NOTE_BYTES 241
#define HOME_FEED_URL_BYTES 513
typedef enum { HOME_WEATHER=0, HOME_FEED=1, HOME_NOTE=2, HOME_SKY=3, HOME_AIR=4 } home_screen_t;
typedef enum { HOME_FIXED=0, HOME_DAY=1, HOME_ROTATE=2 } home_mode_t;
/* HOME_CYCLE is stored in the config only; drawing always gets one of the first three. */
typedef enum { HOME_PRINT=0, HOME_RHYTHM=1, HOME_ATLAS=2, HOME_CYCLE=3 } home_style_t;
typedef enum { HOME_EMPTY=0, HOME_READY=1, HOME_STALE=2, HOME_ERROR=3 } home_source_state_t;
/* Power mode: Breath turns Wi-Fi off between fetches and opens the panel for five minutes after a
 * press of OK; Open keeps Wi-Fi connected and the panel reachable at any time. */
typedef enum { HOME_POWER_BREATH=0, HOME_POWER_OPEN=1 } home_power_mode_t;
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
    bool enabled[HOME_SCREEN_COUNT];
    uint8_t order[HOME_SCREEN_COUNT];
    uint8_t style[HOME_SCREEN_COUNT];
    uint8_t texture; /* cell size 1,2,4 */
    uint8_t intensity; /* 0,1,2 */
    bool large_text, clock24;
    home_mode_t mode;
    uint8_t fixed_screen;
    uint16_t interval_min, pause_min;
    uint16_t cycle_min; /* "In turn": minutes per composition while a screen stays */
    uint8_t ok_action;  /* short OK/BOOT: 0 the "emini" card, 1 refresh, 2 hold, 3 setup window */
    uint8_t air_main;   /* Air: headline number, 0 European index, 1 US AQI, 2 PM2.5 */
    uint8_t brush;      /* tone structure: 0 grain, 1 halftone, 2 grid */
    uint8_t power_mode; /* home_power_mode_t */
    bool quiet_enabled;
    uint16_t quiet_start, quiet_end;
    uint8_t weekdays; /* Monday bit0 */
    uint16_t day_minute[HOME_DAY_SLOTS];
    uint8_t day_screen[HOME_DAY_SLOTS];
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
/* Open-Meteo Air Quality. Index 0 is the last full hour at or before now and is
 * never more than one hour behind it; up to 24 hours are kept from there. An
 * absent series, a JSON null and a physically impossible number all yield NAN
 * (-1 for the integer indices). Pollen is null outside Europe, so pollen[] is
 * NAN there. meta.issued_at is the hour of index 0: the response carries no
 * model issue time of its own. Parser and levels: home_air.h. */
#define HOME_AIR_HOURS 24
#define HOME_POLLEN_COUNT 4
enum {
    HOME_POLLEN_ALDER = 0,
    HOME_POLLEN_BIRCH = 1,
    HOME_POLLEN_GRASS = 2,
    HOME_POLLEN_MUGWORT = 3
};
typedef struct {
    home_source_meta_t meta;
    int64_t forecast_at; /* UTC full hour of index 0 */
    double hourly_pm2_5[HOME_AIR_HOURS], hourly_uv[HOME_AIR_HOURS]; /* NAN when absent */
    uint8_t hourly_count;
    double pm2_5, pm10, uv_index; /* hour of index 0 */
    int16_t european_aqi, us_aqi; /* -1 when absent */
    double pollen[HOME_POLLEN_COUNT]; /* alder, birch, grass, mugwort */
} home_air_t;
typedef struct {
    home_weather_t weather;
    home_feed_t feed;
    home_air_t air;
} home_data_t;

/* Counters kept across restarts, written at most once every few minutes. */
typedef struct {
    int64_t first_start;   /* UTC of the first start with a valid clock */
    uint32_t pictures;     /* full picture changes */
    uint32_t fetches;      /* downloads that came back usable */
    uint32_t awake_minutes;
    int64_t day_stamp;     /* UTC midnight of battery_day[0] */
    int8_t battery_day[7];
} home_counters_t;

/* What the device knows about itself, for the "emini" card (0.6). Counters survive a restart;
 * battery_day[0] is today, [6] is six days ago, -1 where no reading was kept. */
#define HOME_BATTERY_DAYS 7

/* The power log: one entry per hour, one week back.
 * 16 bytes per entry, 2692 bytes for the whole struct - it fits in an NVS record (8 KiB)
 * with plenty to spare. Saved together with the counters, so flash sees nothing new.
 * Why: a discharge curve otherwise needs a computer polling the device from outside. For
 * anyone to measure their own device - and to compare runs without a computer next to it -
 * the device has to be able to tell its own day. */
#define HOME_POWER_HOURS 168
enum {
    HOME_POWER_CHARGING = 1u << 0,
    HOME_POWER_USB = 1u << 1,
    HOME_POWER_LOCKED = 1u << 2, /* the locks held the chip awake for most of the hour */
};
typedef struct {
    int32_t at;           /* UTC start of the hour; 0 = empty entry */
    int16_t millivolts;   /* last trustworthy reading in this hour; 0 = none */
    uint8_t flags;
    uint8_t pictures;     /* full paper refreshes in this hour, capped at 255 */
    uint16_t fetches;     /* usable downloads */
    uint16_t held_s;      /* seconds in which the chip could not sleep */
    /* The drivers of the current draw are stored DIRECTLY, not as event counters. Without them a
     * fit of the coefficients cannot separate the cost of the paper from the cost of the radio
     * under steady use - every hour looks the same and a regression has nothing to tell the
     * parts apart by. */
    uint16_t panel_s;     /* seconds of paper work */
    uint16_t radio_s;     /* seconds with the radio on: the whole hour in Open mode, less in Breath */
} home_power_hour_t;
typedef struct {
    uint16_t written;     /* how many entries were ever written */
    uint16_t head;        /* next slot */
    home_power_hour_t hour[HOME_POWER_HOURS];
} home_power_log_t;
typedef struct {
    int64_t first_start;   /* UTC of the first start we know of; 0 when unknown */
    uint32_t pictures;     /* full picture changes since then */
    uint32_t fetches;      /* downloads that came back usable */
    uint32_t awake_hours;  /* hours with the power on */
    uint32_t render_ms, refresh_ms; /* the last picture: drawing and panel time */
    int8_t battery_day[HOME_BATTERY_DAYS];
    int percent;           /* now, -1 when unknown */
    int estimate_hours;    /* -1 until the device has watched itself long enough */
    bool charging, full;
    /* Numbers about the device itself, not about the data - the second face of the card (0.6). */
    uint32_t wakes_per_hour; /* how many times the main loop wakes per hour */
    int sleep_percent;       /* what share of the time the chip may sleep; -1 when unknown */
    uint32_t uptime_s;
    char address[40];      /* the panel's own name on the home network */
} home_stats_t;

/* The "emini" card has two faces. The first is for showing people: the battery, the wordmark
 * and the QR code - the photo someone posts. The second is for the curious: every counter and
 * a week of battery. Each short press moves to the next face, the third takes the card away -
 * never a redraw on a timer, because every picture costs 25 seconds of paper. */
enum { HOME_INFO_FRONT = 0, HOME_INFO_NERD = 1, HOME_INFO_FACES = 2 };

/* Pure C API; packed B0/W1/Y2/R3, four pixels MSB first. */
void home_render(const home_config_t *config, const home_data_t *data,
                 home_screen_t screen, int64_t now, uint8_t frame[HOME_FRAME_BYTES]);
/* Display language of a saved locale: 0 English, 1 Polish, 2 Chinese. English is the
 * fallback, so an unknown locale never leaves a screen empty. */
int home_language(const char *locale);
void home_render_setup(const char *ssid, const char *password, const char *code,
                       const char *address, int lang, uint8_t frame[HOME_FRAME_BYTES]);
void home_render_status(const char *title, const char *body, int lang,
                       uint8_t frame[HOME_FRAME_BYTES]);
void home_render_info(const home_config_t *config, const home_stats_t *stats, int64_t now, int face,
                      uint8_t frame[HOME_FRAME_BYTES]);
#ifdef HOME_TESTCARD
/* Measurement cards 0..HOME_TESTCARDS-1 (-DHOME_TESTCARD=1); not in release images. */
#define HOME_TESTCARDS 3
void home_render_testcard(int card, uint8_t frame[HOME_FRAME_BYTES]);
#endif
#endif
