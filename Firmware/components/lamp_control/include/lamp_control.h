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
 * Get the current operating mode (MODE_MANUAL, MODE_AUTO, MODE_FLAME).
 */
uint8_t lamp_control_get_mode(void);

/**
 * Switch to a new operating mode.  Stops/starts mode-specific tasks.
 */
void lamp_control_set_mode(uint8_t mode);

/**
 * Apply a scene immediately (used from BLE writes and NVS restore).
 */
void lamp_control_apply_scene(const scene_t *scene);

/**
 * Update auto-mode config while auto mode is running.
 */
void lamp_control_update_auto_config(const auto_config_t *cfg);

#ifdef __cplusplus
}
#endif
