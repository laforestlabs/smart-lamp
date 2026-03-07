#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void circadian_mode_init(void);
void circadian_mode_enable(void);
void circadian_mode_disable(void);
bool circadian_mode_is_active(void);

/**
 * Set wall-clock time from a Unix epoch (seconds since 1970-01-01 UTC).
 * Called from BLE time-sync characteristic write.
 */
void circadian_mode_set_time(uint32_t unix_epoch);

#ifdef __cplusplus
}
#endif
