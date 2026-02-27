#pragma once

#include <stdint.h>
#include "host/ble_gatt.h"

/* Service UUID: F000AA00-0451-4000-B000-000000000000
 * BLE_UUID128_INIT takes bytes LSB-first (little-endian wire order).
 * MSB order: F0 00 AA 00 04 51 40 00 B0 00 00 00 00 00 00 00
 * LSB order: 00 00 00 00 00 00 00 B0 00 40 51 04 00 AA 00 F0 */
#define SVC_UUID_BASE \
    BLE_UUID128_INIT(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB0, \
                     0x00, 0x40, 0x51, 0x04, 0x00, 0xAA, 0x00, 0xF0)

/* Characteristic UUIDs — bytes [12..13] carry the AA-xx suffix (LSB-first) */
#define CHR_UUID(suffix_hi, suffix_lo) \
    BLE_UUID128_INIT(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB0, \
                     0x00, 0x40, 0x51, 0x04, suffix_lo, suffix_hi, 0x00, 0xF0)

/* Characteristic value handles (populated during registration) */
extern uint16_t g_led_state_handle;
extern uint16_t g_mode_handle;
extern uint16_t g_auto_config_handle;
extern uint16_t g_scene_write_handle;
extern uint16_t g_scene_list_handle;
extern uint16_t g_schedule_write_handle;
extern uint16_t g_schedule_list_handle;
extern uint16_t g_sensor_data_handle;
extern uint16_t g_ota_control_handle;
extern uint16_t g_ota_data_handle;
extern uint16_t g_flame_config_handle;
extern uint16_t g_device_info_handle;
extern uint16_t g_sync_config_handle;

/**
 * Register the GATT service with NimBLE.
 */
int ble_gatt_register(void);

/**
 * Get the GATT service definition array.
 */
const struct ble_gatt_svc_def *ble_gatt_get_svcs(void);
