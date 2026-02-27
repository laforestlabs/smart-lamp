#include "sensor.h"
#include "sensor_internal.h"
#include "driver/gpio.h"
#include "driver/dac_oneshot.h"
#include "esp_log.h"

static const char *TAG = "sensor_pir";

static volatile bool s_motion_active = false;
static dac_oneshot_handle_t s_dac_handle = NULL;
static uint8_t s_sensitivity = PIR_SENS_DEFAULT;

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

    /* PIR sensitivity: IO25 = DAC channel 0 — analog output for BM612 SENS pin.
     * BM612: 0V = max sensitivity, VDD/2 = min sensitivity.
     * DAC 8-bit: 0 = 0V, 255 = VDD (3.3V). Usable range: 0–128 (~VDD/2). */
    dac_oneshot_config_t dac_cfg = {
        .chan_id = DAC_CHAN_0,   /* DAC_CHAN_0 = GPIO25 on ESP32 */
    };
    esp_err_t ret = dac_oneshot_new_channel(&dac_cfg, &s_dac_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DAC init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Set default sensitivity via DAC */
    sensor_pir_set_sensitivity(PIR_SENS_DEFAULT);

    /* Install ISR */
    gpio_isr_handler_add(PIR_SIGNAL_GPIO, pir_isr_handler, (void *)event_queue);

    /* Read initial state */
    s_motion_active = (gpio_get_level(PIR_SIGNAL_GPIO) != 0);

    ESP_LOGI(TAG, "PIR sensor initialised (IO%d input, IO%d DAC sens=%u/31)",
             PIR_SIGNAL_GPIO, PIR_SENS_GPIO, s_sensitivity);
    return ESP_OK;
}

esp_err_t sensor_pir_set_sensitivity(uint8_t level)
{
    if (level > PIR_SENS_MAX) level = PIR_SENS_MAX;
    s_sensitivity = level;

    /* Invert: level 31 (max) → DAC 0 (0V, highest BM612 sensitivity)
     *         level 0  (min) → DAC 124 (~VDD/2, lowest BM612 sensitivity) */
    uint8_t dac_val = (uint8_t)((PIR_SENS_MAX - level) * 4);
    esp_err_t ret = dac_oneshot_output_voltage(s_dac_handle, dac_val);
    ESP_LOGI(TAG, "PIR sensitivity set to %u/31 (DAC=%u)", level, dac_val);
    return ret;
}

uint8_t sensor_pir_get_sensitivity(void)
{
    return s_sensitivity;
}

bool sensor_pir_get_motion(void)
{
    return s_motion_active;
}
