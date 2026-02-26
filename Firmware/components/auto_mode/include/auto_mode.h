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
 * Auto mode transition types, used in the transition callback.
 */
typedef enum {
    AUTO_TRANSITION_ON,
    AUTO_TRANSITION_DIMMING,
    AUTO_TRANSITION_OFF,
} auto_transition_t;

/**
 * Callback invoked on auto mode state transitions.
 * @param transition  The transition type.
 * @param dim_master  Dimmed master brightness (only meaningful for DIMMING).
 */
typedef void (*auto_mode_transition_cb_t)(auto_transition_t transition,
                                          uint8_t dim_master);

/**
 * Set the transition callback (call before auto_mode_enable).
 */
void auto_mode_set_transition_cb(auto_mode_transition_cb_t cb);

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
