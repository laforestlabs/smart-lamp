#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialise the NimBLE stack and register the GATT service.
 * Does NOT start advertising.
 */
esp_err_t ble_init(void);

/**
 * Start BLE advertising (connectable, undirected).
 * Called on AT42QT1010 long press.  Times out after 60 s.
 */
void ble_start_advertising(void);

/**
 * Stop BLE advertising.
 */
void ble_stop_advertising(void);

/**
 * Check if a BLE client is currently connected.
 */
bool ble_is_connected(void);

/**
 * Send a notification on the LED State characteristic.
 * Called after lamp_flush() to update the app's live preview.
 */
void ble_notify_led_state(void);

/**
 * Send a notification on the Sensor Data characteristic.
 */
void ble_notify_sensor_data(void);

/**
 * Send a notification on the Scene List characteristic.
 */
void ble_notify_scene_list(void);

/**
 * Send a notification on the Schedule List characteristic.
 */
void ble_notify_schedule_list(void);

/**
 * Send a notification on the OTA Control characteristic (status byte).
 */
void ble_notify_ota_status(uint8_t status);

#ifdef __cplusplus
}
#endif
