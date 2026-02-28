#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "lamp_nvs.h"
#include "lamp_ota.h"
#include "led_driver.h"
#include "sensor.h"
#include "esp_now_sync.h"
#include "ble_service.h"
#include "lamp_control.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Smart Lamp firmware starting");

    /* 1. Initialise NVS — must be first (BLE bonds + app data live here) */
    ESP_ERROR_CHECK(lamp_nvs_init());

    /* 2. Check OTA rollback status */
    lamp_ota_check_rollback();

    /* 3. Initialise the LED driver (RMT channel on IO19) and clear any residual state */
    ESP_ERROR_CHECK(led_driver_init());
    lamp_off();

    /* 4. Initialise sensors — creates the shared event queue */
    QueueHandle_t sensor_queue = xQueueCreate(16, sizeof(sensor_event_t));
    assert(sensor_queue);
    ESP_ERROR_CHECK(sensor_init(sensor_queue));

    /* 5. Initialise BLE stack (before WiFi — BT controller must init first on ESP32) */
    ESP_ERROR_CHECK(ble_init());

    /* 6. Initialise ESP-NOW sync (WiFi STA + ESP-NOW, after BLE for coexistence) */
    ESP_ERROR_CHECK(esp_now_sync_init());

    /* 7. Start the central lamp controller (creates its own task) */
    ESP_ERROR_CHECK(lamp_control_init(sensor_queue));

    ESP_LOGI(TAG, "Initialisation complete");
}
