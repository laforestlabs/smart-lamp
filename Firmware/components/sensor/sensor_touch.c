#include "sensor.h"
#include "sensor_internal.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "sensor_touch";

#define LONG_PRESS_US   (3000000)   /* 3 seconds */
#define DEBOUNCE_US     (50000)     /* 50 ms */

static QueueHandle_t    s_queue;
static esp_timer_handle_t s_long_timer;
static int64_t          s_last_edge_us;
static bool             s_long_fired;

static void long_press_cb(void *arg)
{
    /* Timer fired while still pressed → long press */
    if (gpio_get_level(TOUCH_OUT_GPIO)) {
        s_long_fired = true;
        sensor_event_t evt = { .type = SENSOR_EVT_TOUCH_LONG };
        xQueueSend(s_queue, &evt, 0);
    }
}

static void IRAM_ATTR touch_isr_handler(void *arg)
{
    int64_t now = esp_timer_get_time();

    /* Debounce */
    if ((now - s_last_edge_us) < DEBOUNCE_US) return;
    s_last_edge_us = now;

    int level = gpio_get_level(TOUCH_OUT_GPIO);

    if (level) {
        /* Rising edge: touch started — start long-press timer */
        s_long_fired = false;
        esp_timer_start_once(s_long_timer, LONG_PRESS_US);
    } else {
        /* Falling edge: touch released */
        esp_timer_stop(s_long_timer);
        if (!s_long_fired) {
            /* Short tap */
            sensor_event_t evt = { .type = SENSOR_EVT_TOUCH_SHORT };
            xQueueSendFromISR(s_queue, &evt, NULL);
        }
    }
}

esp_err_t sensor_touch_init(QueueHandle_t event_queue)
{
    s_queue = event_queue;

    /* AT42QT1010 OUT: IO16 input, interrupt on both edges */
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << TOUCH_OUT_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&cfg);

    /* Long-press timer (runs in timer task context, not ISR) */
    esp_timer_create_args_t timer_args = {
        .callback = long_press_cb,
        .name     = "touch_long",
    };
    esp_timer_create(&timer_args, &s_long_timer);

    gpio_isr_handler_add(TOUCH_OUT_GPIO, touch_isr_handler, NULL);

    ESP_LOGI(TAG, "Touch sensor initialised (IO%d)", TOUCH_OUT_GPIO);
    return ESP_OK;
}
