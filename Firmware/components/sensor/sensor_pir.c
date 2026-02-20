#include "sensor.h"
#include "sensor_internal.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "sensor_pir";

static volatile bool s_motion_active = false;

static void IRAM_ATTR pir_isr_handler(void *arg)
{
    QueueHandle_t queue = (QueueHandle_t)arg;
    int level = gpio_get_level(PIR_SIGNAL_GPIO);
    s_motion_active = (level != 0);

    sensor_event_t evt = {
        .type = level ? SENSOR_EVT_MOTION_START : SENSOR_EVT_MOTION_END,
    };
    xQueueSendFromISR(queue, &evt, NULL);
}

esp_err_t sensor_pir_init(QueueHandle_t event_queue)
{
    /* PIR output: IO27 input, interrupt on both edges */
    gpio_config_t pir_cfg = {
        .pin_bit_mask = 1ULL << PIR_SIGNAL_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLDOWN_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&pir_cfg);

    /* PIR sensitivity: IO25 output, drive HIGH for maximum sensitivity */
    gpio_config_t sens_cfg = {
        .pin_bit_mask = 1ULL << PIR_SENS_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&sens_cfg);
    gpio_set_level(PIR_SENS_GPIO, 1);

    /* Install ISR */
    gpio_isr_handler_add(PIR_SIGNAL_GPIO, pir_isr_handler, (void *)event_queue);

    /* Read initial state */
    s_motion_active = (gpio_get_level(PIR_SIGNAL_GPIO) != 0);

    ESP_LOGI(TAG, "PIR sensor initialised (IO%d input, IO%d sens=HIGH)",
             PIR_SIGNAL_GPIO, PIR_SENS_GPIO);
    return ESP_OK;
}

bool sensor_pir_get_motion(void)
{
    return s_motion_active;
}
