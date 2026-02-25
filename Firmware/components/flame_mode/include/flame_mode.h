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
 * Set the base colour for the flame effect.
 * The flame animation modulates intensity while keeping this colour ratio
 * constant across all pixels.  Call this when the active scene changes.
 */
void flame_mode_set_color(uint8_t warm, uint8_t neutral, uint8_t cool);

/**
 * Returns true if the flame task is currently running.
 */
bool flame_mode_is_active(void);

#ifdef __cplusplus
}
#endif
