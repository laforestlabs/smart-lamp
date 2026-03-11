#include "sensor.h"
#include "sensor_internal.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "sensor_touch";

/*
 * Polling-based touch detection with integrating debounce.
 *
 * The AT42QT1010 OUT pin can oscillate when electrode capacitance is near the
 * detection threshold.  Instead of reacting to every edge (ISR-based), we poll
 * at a fixed 20 ms interval and require DEBOUNCE_THRESH consecutive readings
 * in the same direction before registering a state change.  This makes the
 * driver immune to rapid oscillation.
 */

#define POLL_INTERVAL_US    (20000)     /* 20 ms polling period */
#define DEBOUNCE_THRESH     (5)         /* 5 polls = 100 ms of stable signal */
#define LONG_PRESS_US       (3000000)   /* 3 s for long press */
#define MIN_PRESS_US        (150000)    /* 150 ms minimum valid press */
#define LOCKOUT_US          (500000)    /* 500 ms post-event lockout */

static QueueHandle_t      s_queue;
static esp_timer_handle_t s_poll_timer;
static esp_timer_handle_t s_long_timer;

/* Debounce state */
static int      s_integrator;       /* counts up when HIGH, down when LOW */
static bool     s_debounced_state;  /* stable output after filtering */

/* Press timing */
static int64_t  s_press_start_us;
static int64_t  s_last_event_us;
static bool     s_long_fired;

static void long_press_cb(void *arg)
{
    /* Timer fired while still pressed → long press */
    if (s_debounced_state) {
        s_long_fired = true;
        sensor_event_t evt = { .type = SENSOR_EVT_TOUCH_LONG };
        xQueueSend(s_queue, &evt, 0);
        ESP_LOGI(TAG, "Long press detected");
    }
}

static void poll_cb(void *arg)
{
    int raw = gpio_get_level(TOUCH_OUT_GPIO);

    /* Integrating debounce: move counter toward the raw reading */
    if (raw) {
        if (s_integrator < DEBOUNCE_THRESH)
            s_integrator++;
    } else {
        if (s_integrator > 0)
            s_integrator--;
    }

    /* Check for state transition */
    if (!s_debounced_state && s_integrator >= DEBOUNCE_THRESH) {
        /* ── Pressed ── */
        s_debounced_state = true;
        s_press_start_us = esp_timer_get_time();
        s_long_fired = false;
        esp_timer_stop(s_long_timer);
        esp_timer_start_once(s_long_timer, LONG_PRESS_US);
        ESP_LOGD(TAG, "Touch started");

    } else if (s_debounced_state && s_integrator == 0) {
        /* ── Released ── */
        s_debounced_state = false;
        esp_timer_stop(s_long_timer);

        int64_t now = esp_timer_get_time();
        int64_t held = now - s_press_start_us;

        if (!s_long_fired &&
            held >= MIN_PRESS_US &&
            (now - s_last_event_us) >= LOCKOUT_US) {
            s_last_event_us = now;
            sensor_event_t evt = { .type = SENSOR_EVT_TOUCH_SHORT };
            xQueueSend(s_queue, &evt, 0);
            ESP_LOGI(TAG, "Short press detected (held %lld ms)", held / 1000);
        }
    }
}

esp_err_t sensor_touch_init(QueueHandle_t event_queue)
{
    s_queue = event_queue;

    /* AT42QT1010 OUT: IO16 input, no interrupts — polled instead */
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << TOUCH_OUT_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    /* Long-press timer */
    esp_timer_create_args_t long_args = {
        .callback = long_press_cb,
        .name     = "touch_long",
    };
    esp_timer_create(&long_args, &s_long_timer);

    /* Polling timer — runs every 20 ms */
    esp_timer_create_args_t poll_args = {
        .callback = poll_cb,
        .name     = "touch_poll",
    };
    esp_timer_create(&poll_args, &s_poll_timer);
    esp_timer_start_periodic(s_poll_timer, POLL_INTERVAL_US);

    ESP_LOGI(TAG, "Touch sensor initialised (IO%d, polled every %d ms, thresh=%d)",
             TOUCH_OUT_GPIO, POLL_INTERVAL_US / 1000, DEBOUNCE_THRESH);
    return ESP_OK;
}
