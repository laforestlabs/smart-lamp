#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialise WiFi STA (no connection) + ESP-NOW.
 * Must be called after lamp_nvs_init() and before ble_init().
 */
esp_err_t esp_now_sync_init(void);

/**
 * Broadcast current lamp state to group peers.
 * Called from lamp_control when state changes from a LOCAL source.
 * Non-blocking: overwrites any pending message in the TX queue.
 */
void esp_now_sync_broadcast(uint8_t warm, uint8_t neutral, uint8_t cool,
                            uint8_t master, uint8_t flags);

/** Get current group ID (0 = disabled). */
uint8_t esp_now_sync_get_group(void);

/** Set group ID and persist to NVS. 0 = leave group / disable sync. */
esp_err_t esp_now_sync_set_group(uint8_t group_id);

/** Get this device's WiFi STA MAC (for display in app). */
void esp_now_sync_get_mac(uint8_t mac_out[6]);

#ifdef __cplusplus
}
#endif
