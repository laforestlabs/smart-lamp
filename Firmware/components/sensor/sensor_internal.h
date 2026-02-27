#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/* Per-sensor init functions (called from sensor_init) */
esp_err_t sensor_pir_init(QueueHandle_t event_queue);
esp_err_t sensor_touch_init(QueueHandle_t event_queue);
esp_err_t sensor_light_init(QueueHandle_t event_queue);

/* Per-sensor accessors */
bool    sensor_pir_get_motion(void);
uint8_t sensor_light_get_lux(void);

/* PIR sensitivity (DAC-driven) */
esp_err_t sensor_pir_set_sensitivity(uint8_t level);
uint8_t   sensor_pir_get_sensitivity(void);
