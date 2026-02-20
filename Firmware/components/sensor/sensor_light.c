#include "sensor.h"
#include "sensor_internal.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "sensor_light";

#define SAMPLE_INTERVAL_US  (1000000)   /* 1 second */
#define MEDIAN_WINDOW       5

/* IO17 = ADC1_CHANNEL_7 on ESP32 */
#define LIGHT_ADC_CHANNEL   ADC_CHANNEL_7

static QueueHandle_t        s_queue;
static adc_oneshot_unit_handle_t s_adc_handle;
static esp_timer_handle_t   s_timer;
static volatile uint8_t     s_lux;

/* Median filter buffer */
static int s_samples[MEDIAN_WINDOW];
static int s_sample_idx;
static bool s_buffer_full;

static int compare_int(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static int median_filter(int new_sample)
{
    s_samples[s_sample_idx] = new_sample;
    s_sample_idx = (s_sample_idx + 1) % MEDIAN_WINDOW;
    if (s_sample_idx == 0) s_buffer_full = true;

    int count = s_buffer_full ? MEDIAN_WINDOW : s_sample_idx;
    int sorted[MEDIAN_WINDOW];
    for (int i = 0; i < count; i++) sorted[i] = s_samples[i];
    qsort(sorted, count, sizeof(int), compare_int);
    return sorted[count / 2];
}

static void adc_sample_cb(void *arg)
{
    int raw;
    if (adc_oneshot_read(s_adc_handle, LIGHT_ADC_CHANNEL, &raw) != ESP_OK) return;

    raw = median_filter(raw);

    /*
     * Phototransistor with pull-up: more light → lower voltage → lower ADC value.
     * Map: ADC 0 (bright) → lux 100, ADC 4095 (dark) → lux 0.
     */
    int lux = 100 - (raw * 100 / 4095);
    if (lux < 0) lux = 0;
    if (lux > 100) lux = 100;
    s_lux = (uint8_t)lux;

    sensor_event_t evt = {
        .type = SENSOR_EVT_LUX_UPDATE,
        .data.lux = s_lux,
    };
    xQueueSend(s_queue, &evt, 0);
}

esp_err_t sensor_light_init(QueueHandle_t event_queue)
{
    s_queue = event_queue;

    /* Configure ADC1 */
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_cfg, &s_adc_handle), TAG, "ADC unit init failed");

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_12,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc_handle, LIGHT_ADC_CHANNEL, &chan_cfg),
                        TAG, "ADC channel config failed");

    /* Periodic sampling timer */
    esp_timer_create_args_t timer_args = {
        .callback = adc_sample_cb,
        .name     = "light_adc",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_timer), TAG, "timer create failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(s_timer, SAMPLE_INTERVAL_US), TAG, "timer start failed");

    ESP_LOGI(TAG, "Ambient light sensor initialised (IO%d, ADC1_CH%d)",
             LIGHT_ADC_GPIO, LIGHT_ADC_CHANNEL);
    return ESP_OK;
}

uint8_t sensor_light_get_lux(void)
{
    return s_lux;
}
