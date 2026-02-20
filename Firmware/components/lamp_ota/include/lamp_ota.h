#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OTA control commands (from BLE) */
#define OTA_CMD_START   0x01
#define OTA_CMD_END     0x02
#define OTA_CMD_ABORT   0xFF

/* OTA status (notified back via BLE) */
#define OTA_STATUS_READY    0x00
#define OTA_STATUS_BUSY     0x01
#define OTA_STATUS_OK       0x02
#define OTA_STATUS_ERROR    0x03

/**
 * Check OTA rollback status on boot.
 * If we booted into a new partition that hasn't been validated, mark it valid.
 */
void lamp_ota_check_rollback(void);

/**
 * Begin an OTA update.
 * @return ESP_OK if ready to receive data.
 */
esp_err_t lamp_ota_begin(void);

/**
 * Write a chunk of firmware data.
 * @param data  Pointer to the data.
 * @param len   Length in bytes.
 */
esp_err_t lamp_ota_write_chunk(const uint8_t *data, size_t len);

/**
 * Finish the OTA update, validate the image, and set boot partition.
 * Does NOT reboot — caller should reboot after notifying the app.
 */
esp_err_t lamp_ota_finish(void);

/**
 * Abort an in-progress OTA update.
 */
void lamp_ota_abort(void);

/**
 * Returns true if an OTA update is currently in progress.
 */
bool lamp_ota_in_progress(void);

#ifdef __cplusplus
}
#endif
