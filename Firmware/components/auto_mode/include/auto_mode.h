#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "lamp_nvs.h"
#include "sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AUTO_STATE_IDLE,
    AUTO_STATE_ON,
    AUTO_STATE_DIMMING,
} auto_state_t;

/**
 * Initialise the auto-mode module (loads config from NVS).
 */
esp_err_t auto_mode_init(void);

/**
 * Enable auto mode — the state machine begins processing sensor events.
 */
void auto_mode_enable(void);

/**
 * Disable auto mode — stops the state machine, does NOT change LEDs.
 */
void auto_mode_disable(void);

/**
 * Feed a sensor event to the auto-mode state machine.
 * Called from the lamp_control event loop.
 */
void auto_mode_process_event(const sensor_event_t *evt);

/**
 * Get the current state machine state.
 */
auto_state_t auto_mode_get_state(void);

/**
 * Update auto config at runtime (also saves to NVS).
 */
esp_err_t auto_mode_set_config(const auto_config_t *cfg);

/**
 * Get current config.
 */
void auto_mode_get_config(auto_config_t *cfg);

#ifdef __cplusplus
}
#endif
