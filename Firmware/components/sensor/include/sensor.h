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

typedef enum {
    SENSOR_EVT_MOTION_START,
    SENSOR_EVT_MOTION_END,
    SENSOR_EVT_TOUCH_SHORT,
    SENSOR_EVT_TOUCH_LONG,
    SENSOR_EVT_LUX_UPDATE,
} sensor_event_type_t;

typedef struct {
    sensor_event_type_t type;
    union {
        uint8_t lux;            /* 0–100 for LUX_UPDATE */
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

#ifdef __cplusplus
}
#endif
