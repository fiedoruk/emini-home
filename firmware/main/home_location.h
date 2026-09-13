#ifndef HOME_LOCATION_H
#define HOME_LOCATION_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "cJSON.h"

#define HOME_LOCATION_PROVIDER "FreeIPAPI"
typedef struct {
    double latitude, longitude;
    char city[65], country[3], timezone[49];
} home_area_t;
typedef enum {
    HOME_AREA_IDLE,
    HOME_AREA_PENDING,
    HOME_AREA_READY,
    HOME_AREA_ERROR
} home_area_phase_t;
typedef struct {
    home_area_phase_t phase;
    bool requested, running, has_request;
    uint32_t generation;
    int64_t requested_us, last_request_us;
    home_area_t area;
    char error[48];
} home_location_t;

/* Pure parser: accepts only selected fields, never returns/logs public IP.
 * timeZones is a country list: used only when it holds exactly one known zone. */
bool home_location_parse(const char *json, size_t size, home_area_t *out);
/* Caller holds the runtime lock for queue/state operations. RAM only. */
bool home_location_enqueue(home_location_t *, int64_t now_us, bool online,
                           bool clock_valid, bool maintenance, const char **error);
bool home_location_take(home_location_t *, int64_t now_us, uint32_t *generation);
void home_location_expire(home_location_t *,int64_t now_us);
void home_location_finish(home_location_t *, uint32_t generation,
                          const home_area_t *area, const char *error);
cJSON *home_location_json(const home_location_t *);
#endif
