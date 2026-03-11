#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GPIO assignments */
#define PIR_SIGNAL_GPIO     27
#define PIR_SENS_GPIO       25
#define TOUCH_OUT_GPIO      16
#define LIGHT_ADC_GPIO      17

/* PIR sensitivity: 0 = least sensitive, 31 = most sensitive */
#define PIR_SENS_MAX        31
#define PIR_SENS_DEFAULT    24

typedef enum {
    SENSOR_EVT_MOTION_START,
    SENSOR_EVT_MOTION_END,
    SENSOR_EVT_TOUCH_SHORT,
    SENSOR_EVT_TOUCH_LONG,
    SENSOR_EVT_LUX_UPDATE,
    SENSOR_EVT_SYNC,            /* ESP-NOW state received from peer */
    SENSOR_EVT_AUTO_UNSUPPRESS, /* suppress timer expired — re-enable auto mode */
} sensor_event_type_t;

/** Full scene + operational state carried in a SENSOR_EVT_SYNC event. */
typedef struct {
    uint8_t  warm;
    uint8_t  neutral;
    uint8_t  cool;
    uint8_t  master;          /* configured brightness (always the scene target) */
    uint8_t  flags;
    uint8_t  fade_in_s;
    uint8_t  fade_out_s;
    uint16_t auto_timeout_s;
    uint16_t auto_lux_threshold;
    uint16_t auto_suppress_min;
    uint8_t  flame_config[7]; /* drift_x, drift_y, restore, radius, bias_y,
                                  flicker_depth, flicker_speed */
    uint8_t  pir_sensitivity;
    uint8_t  lamp_on;         /* 0 = off, 1 = on (operational state) */
} sensor_sync_data_t;

typedef struct {
    sensor_event_type_t type;
    union {
        uint8_t            lux;  /* 0–100 for LUX_UPDATE */
        sensor_sync_data_t sync; /* SENSOR_EVT_SYNC */
    } data;
} sensor_event_t;

/**
 * Initialise all sensors.  Events are posted to @p event_queue.
 */
esp_err_t sensor_init(QueueHandle_t event_queue);

/**
 * Get the most recent lux reading (0–100).
 */
uint8_t sensor_get_lux(void);

/**
 * Get the current motion state (true = motion detected).
 */
bool sensor_get_motion(void);

/**
 * Set PIR motion sensor sensitivity (0–31).
 * 0 = least sensitive (shortest range), 31 = most sensitive (longest range).
 */
esp_err_t sensor_set_pir_sensitivity(uint8_t level);

/**
 * Get the current PIR sensitivity level (0–31).
 */
uint8_t sensor_get_pir_sensitivity(void);

#ifdef __cplusplus
}
#endif
