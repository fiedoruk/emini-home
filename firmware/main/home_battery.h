#ifndef HOME_BATTERY_H
#define HOME_BATTERY_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool valid, charge_valid, charging, full;
    int millivolts, percent_estimate;
    int64_t measured_at;
} home_battery_t;
void home_battery_task(void *unused);
#endif
