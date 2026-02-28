#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lamp_nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialise the lamp controller and create its event-loop task.
 * @param sensor_queue  The queue that sensor events are posted to.
 */
esp_err_t lamp_control_init(QueueHandle_t sensor_queue);

/**
 * Get the current mode flags bitmask (MODE_FLAG_AUTO | MODE_FLAG_FLAME).
 */
uint8_t lamp_control_get_flags(void);

/**
 * Set mode flags bitmask.  Independently starts/stops auto and flame.
 */
void lamp_control_set_flags(uint8_t flags);

/**
 * Apply a scene immediately (used from BLE writes and NVS restore).
 */
void lamp_control_apply_scene(const scene_t *scene);

/**
 * Update LED state (warm/neutral/cool/master) in a mode-aware way.
 * In manual mode: applies directly to framebuffer.
 * In flame mode:  updates colour ratios without interrupting animation.
 * In auto mode:   saves state for next ON transition.
 */
void lamp_control_set_state(uint8_t warm, uint8_t neutral, uint8_t cool, uint8_t master);

/**
 * Update auto-mode config while auto mode is running.
 */
void lamp_control_update_auto_config(const auto_config_t *cfg);

/**
 * Apply state received from ESP-NOW sync.
 * Same as lamp_control_set_state() but does NOT re-broadcast (prevents loops).
 */
void lamp_control_set_state_from_sync(uint8_t warm, uint8_t neutral,
                                      uint8_t cool, uint8_t master);

/**
 * Apply mode flags received from ESP-NOW sync.
 * Same as lamp_control_set_flags() but does NOT re-broadcast (prevents loops).
 */
void lamp_control_set_flags_from_sync(uint8_t flags);

/**
 * Apply full state + flags from ESP-NOW sync atomically.
 * Sets flags, state, and s_lamp_on together without re-broadcasting.
 */
void lamp_control_apply_sync(uint8_t warm, uint8_t neutral, uint8_t cool,
                              uint8_t master, uint8_t flags);

#ifdef __cplusplus
}
#endif
