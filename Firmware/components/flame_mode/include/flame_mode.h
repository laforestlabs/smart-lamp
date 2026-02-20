#pragma once

#include "esp_err.h"
#include "lamp_nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the flame animation task (30 fps).
 * Loads config from NVS on start.
 */
esp_err_t flame_mode_start(void);

/**
 * Stop the flame animation task and turn off LEDs.
 */
void flame_mode_stop(void);

/**
 * Update flame parameters at runtime (also saves to NVS).
 */
esp_err_t flame_mode_set_config(const flame_config_t *cfg);

/**
 * Get current flame config.
 */
void flame_mode_get_config(flame_config_t *cfg);

/**
 * Returns true if the flame task is currently running.
 */
bool flame_mode_is_active(void);

#ifdef __cplusplus
}
#endif
