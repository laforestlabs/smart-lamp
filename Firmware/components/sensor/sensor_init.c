#include "sensor.h"
#include "sensor_internal.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "sensor";

esp_err_t sensor_init(QueueHandle_t event_queue)
{
    /* Install GPIO ISR service (shared by PIR and touch) */
    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        /* ESP_ERR_INVALID_STATE means already installed — that's fine */
        return ret;
    }

    ESP_RETURN_ON_ERROR(sensor_pir_init(event_queue),   TAG, "PIR init failed");
    ESP_RETURN_ON_ERROR(sensor_touch_init(event_queue), TAG, "Touch init failed");
    ESP_RETURN_ON_ERROR(sensor_light_init(event_queue), TAG, "Light init failed");

    ESP_LOGI(TAG, "All sensors initialised");
    return ESP_OK;
}

uint8_t sensor_get_lux(void)
{
    return sensor_light_get_lux();
}

bool sensor_get_motion(void)
{
    return sensor_pir_get_motion();
}

esp_err_t sensor_set_pir_sensitivity(uint8_t level)
{
    return sensor_pir_set_sensitivity(level);
}

uint8_t sensor_get_pir_sensitivity(void)
{
    return sensor_pir_get_sensitivity();
}
