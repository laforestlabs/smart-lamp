#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lamp_nvs.h"
#include "sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialise the lamp controller and create its event-loop task.
 * @param sensor_queue  The queue that sensor events are posted to.
 */
esp_err_t lamp_control_init(QueueHandle_t sensor_queue);

/**
 * Get the current mode flags bitmask.
 */
uint8_t lamp_control_get_flags(void);

/**
 * Get the active scene's master brightness (0–255).
 */
uint8_t lamp_control_get_master(void);

/**
 * Set mode flags bitmask.  Independently starts/stops auto and flame.
 */
void lamp_control_set_flags(uint8_t flags);

/**
 * Apply a scene immediately (used from BLE writes and NVS restore).
 * Atomically sets all sub-module configs (auto, flame, PIR) and applies LEDs.
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
 * Update auto-mode config (persists to active scene in NVS).
 */
void lamp_control_update_auto_config(const auto_config_t *cfg);

/**
 * Update flame config (persists to active scene in NVS).
 */
void lamp_control_update_flame_config(const flame_config_t *cfg);

/**
 * Set PIR sensitivity level (persists to active scene in NVS).
 */
void lamp_control_set_pir_sensitivity(uint8_t level);

/**
 * Apply a full sync event received from an ESP-NOW peer.
 * Builds a scene from the sync data, applies it via lamp_control_apply_scene(),
 * then handles the operational lamp_on state via auto_mode_force_on/off.
 * Does NOT re-broadcast (prevents sync loops).
 */
void lamp_control_apply_sync(const sensor_sync_data_t *sync);

#ifdef __cplusplus
}
#endif
