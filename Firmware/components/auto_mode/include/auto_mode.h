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
    AUTO_STATE_FADING_IN,
    AUTO_STATE_ON,
    AUTO_STATE_FADING_OUT,
} auto_state_t;

/**
 * Auto mode transition types, used in the transition callback.
 */
typedef enum {
    AUTO_TRANSITION_ON,       /* fade-in starting (dim_master = initial brightness) or instant ON */
    AUTO_TRANSITION_DIMMING,  /* smooth fade tick — set LED master to dim_master */
    AUTO_TRANSITION_OFF,      /* fade-out complete; turn off LEDs */
} auto_transition_t;

/**
 * Callback invoked on auto mode state transitions.
 * @param transition  The transition type.
 * @param dim_master  Brightness value: initial master for ON, current for DIMMING, ignored for OFF.
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
 * Disable auto mode — stops the state machine and any active fade, does NOT change LEDs.
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

/**
 * Set per-scene fade rates (call whenever the active scene changes).
 * @param fade_in_s   Seconds to fade from off to full brightness (0 = instant).
 * @param fade_out_s  Seconds to fade from full brightness to off (0 = instant).
 */
void auto_mode_set_fade_rates(uint8_t fade_in_s, uint8_t fade_out_s);

/**
 * Notify auto mode that the active scene has been updated externally (e.g. via sync).
 * Updates the internal scene copy so fades use the correct target values.
 * If a fade-out is in progress and master > 0, it is aborted and the lamp returns to ON.
 */
void auto_mode_notify_scene_change(uint8_t warm, uint8_t neutral,
                                   uint8_t cool, uint8_t master);

/**
 * Force the lamp on immediately (bypasses PIR lux check).
 * Used for group sync: when a peer's lamp has turned on, mirror that state.
 * No-op if already on or not enabled.
 */
void auto_mode_force_on(void);

/**
 * Force the lamp off immediately (bypasses inactivity timeout).
 * Used for group sync: when a peer's lamp has turned off, mirror that state.
 * No-op if already off or not enabled.
 */
void auto_mode_force_off(void);

#ifdef __cplusplus
}
#endif
