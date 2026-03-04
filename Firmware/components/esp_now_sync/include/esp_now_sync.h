#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lamp_nvs.h"   /* scene_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialise WiFi STA (no connection) + ESP-NOW.
 * Must be called after lamp_nvs_init() and before ble_init().
 * @param sensor_queue  Queue to post SENSOR_EVT_SYNC events to.
 */
esp_err_t esp_now_sync_init(QueueHandle_t sensor_queue);

/**
 * Broadcast the current scene configuration + operational state to group peers.
 * Called from lamp_control when state changes from a LOCAL source.
 * Non-blocking: overwrites any pending message in the TX queue.
 *
 * @param scene   Full active scene (colors, configured master, mode flags,
 *                fade rates, auto/flame config, PIR sensitivity).
 * @param lamp_on Whether the lamp is currently on (operational state).
 *                Kept separate from scene->master so master is never
 *                artificially zeroed to signal "off".
 */
void esp_now_sync_broadcast(const scene_t *scene, bool lamp_on);

/** Get current group ID (0 = disabled). */
uint8_t esp_now_sync_get_group(void);

/** Set group ID and persist to NVS. 0 = leave group / disable sync. */
esp_err_t esp_now_sync_set_group(uint8_t group_id);

/** Get this device's WiFi STA MAC (for display in app). */
void esp_now_sync_get_mac(uint8_t mac_out[6]);

#ifdef __cplusplus
}
#endif
