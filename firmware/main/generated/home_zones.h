/* Generated from the IANA time-zone database. tzdb 2026c; public-domain data. */
#ifndef HOME_ZONES_GENERATED_H
#define HOME_ZONES_GENERATED_H
#include <stddef.h>
#include <stdint.h>
typedef struct { const char *name; const char *label; uint16_t span; } home_zone_row_t;
typedef struct { uint32_t start; uint16_t count; } home_zone_span_t;
typedef struct { int32_t offset; uint8_t dst; } home_zone_state_t;
extern const home_zone_row_t home_zone_rows[];
extern const size_t home_zone_count;
extern const home_zone_span_t home_zone_spans[];
extern const uint32_t home_zone_epochs[];
extern const uint8_t home_zone_state_indices[];
extern const home_zone_state_t home_zone_states[];
#define HOME_ZONE_START INT64_C(0)
#define HOME_ZONE_STOP INT64_C(2240611200)
#define HOME_ZONE_VERSION "2026c"
#endif
