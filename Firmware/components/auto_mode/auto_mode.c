#include "auto_mode.h"
#include "led_driver.h"
#include "lamp_nvs.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "auto_mode";

static auto_config_t   s_cfg;
static auto_state_t    s_state = AUTO_STATE_IDLE;
static bool            s_enabled = false;
static uint8_t         s_current_lux = 100;    /* assume bright until first reading */

/* Per-scene fade durations */
static uint8_t s_fade_in_s  = FADE_IN_S_DEFAULT;
static uint8_t s_fade_out_s = FADE_OUT_S_DEFAULT;

/* Fade state */
static uint8_t  s_fade_current_master = 0;
static uint8_t  s_fade_start_master   = 0;
static uint8_t  s_fade_target_master  = 0;
static int64_t  s_fade_start_us       = 0;
static uint32_t s_fade_duration_us    = 0;

static esp_timer_handle_t s_timeout_timer;   /* one-shot inactivity timer */
static esp_timer_handle_t s_fade_timer;      /* periodic 50 ms fade tick  */
static esp_timer_handle_t s_suppress_timer;  /* touch-off suppress timer  */
static bool               s_suppressed = false;
static QueueHandle_t      s_suppress_queue;  /* sensor queue for unsuppress event */

/* Saved scene to restore when entering ON state */
static scene_t s_active_scene;

/* Transition callback — set by lamp_control */
static auto_mode_transition_cb_t s_transition_cb = NULL;

void auto_mode_set_transition_cb(auto_mode_transition_cb_t cb)
{
    s_transition_cb = cb;
}

void auto_mode_set_fade_rates(uint8_t fade_in_s, uint8_t fade_out_s)
{
    s_fade_in_s  = fade_in_s;
    s_fade_out_s = fade_out_s;
}

/* ── Forward declarations ── */
static void start_fade_in(bool prep_buffer);
static void start_fade_out(void);

/* ── Suppress timer callback ── */

static void suppress_cb(void *arg)
{
    ESP_LOGI(TAG, "Suppress timer expired → posting unsuppress event");
    s_suppressed = false;
    sensor_event_t evt = { .type = SENSOR_EVT_AUTO_UNSUPPRESS };
    xQueueSend(s_suppress_queue, &evt, 0);
}

/* ── Inactivity timeout callback ── */

static void timeout_cb(void *arg)
{
    if (s_state == AUTO_STATE_ON) {
        ESP_LOGI(TAG, "Inactivity timeout → fade out (fade_out_s=%u)", s_fade_out_s);
        start_fade_out();
    }
}

/* ── Periodic fade tick ── */

static void fade_timer_cb(void *arg)
{
    int64_t elapsed = esp_timer_get_time() - s_fade_start_us;
    float progress = (s_fade_duration_us > 0)
                     ? (float)elapsed / (float)s_fade_duration_us
                     : 1.0f;
    if (progress > 1.0f) progress = 1.0f;

    float master_f = s_fade_start_master
                     + (s_fade_target_master - s_fade_start_master) * progress;
    uint8_t m = (uint8_t)(master_f + 0.5f);
    s_fade_current_master = m;

    if (s_transition_cb) {
        s_transition_cb(AUTO_TRANSITION_DIMMING, m);
    }

    if (progress >= 1.0f) {
        esp_timer_stop(s_fade_timer);

        if (s_state == AUTO_STATE_FADING_OUT) {
            ESP_LOGI(TAG, "Fade out complete → IDLE");
            s_state = AUTO_STATE_IDLE;
            if (s_transition_cb) {
                s_transition_cb(AUTO_TRANSITION_OFF, 0);
            }
        } else if (s_state == AUTO_STATE_FADING_IN) {
            ESP_LOGI(TAG, "Fade in complete → ON");
            s_state = AUTO_STATE_ON;
            /* Notify lamp_control that we're fully on (s_lamp_on tracking) */
            if (s_transition_cb) {
                s_transition_cb(AUTO_TRANSITION_ON, s_active_scene.master);
            }
            /* Start inactivity timer */
            esp_timer_start_once(s_timeout_timer,
                                 (uint64_t)s_cfg.timeout_s * 1000000);
        }
    }
}

/* ── Fade helpers ── */

static void start_fade_in(bool prep_buffer)
{
    uint8_t target = s_active_scene.master;

    if (s_fade_in_s == 0) {
        /* Instant ON */
        s_fade_current_master = target;
        s_state = AUTO_STATE_ON;
        if (s_transition_cb) {
            s_transition_cb(AUTO_TRANSITION_ON, target);
        }
        esp_timer_start_once(s_timeout_timer,
                             (uint64_t)s_cfg.timeout_s * 1000000);
        ESP_LOGI(TAG, "Instant ON (master=%u)", target);
        return;
    }

    /* Prepare the LED buffer with scene colors at the current brightness */
    if (prep_buffer && s_transition_cb) {
        s_transition_cb(AUTO_TRANSITION_ON, s_fade_current_master);
    }

    s_fade_start_master  = s_fade_current_master;
    s_fade_target_master = target;
    s_fade_duration_us   = (uint32_t)s_fade_in_s * 1000000UL;
    s_fade_start_us      = esp_timer_get_time();
    s_state              = AUTO_STATE_FADING_IN;

    esp_timer_start_periodic(s_fade_timer, 50000);  /* 50 ms */
    ESP_LOGI(TAG, "Fade in: %u→%u over %us", s_fade_start_master, target, s_fade_in_s);
}

static void start_fade_out(void)
{
    uint8_t from = s_active_scene.master;

    if (s_fade_out_s == 0) {
        /* Instant OFF */
        s_fade_current_master = 0;
        s_state = AUTO_STATE_IDLE;
        if (s_transition_cb) {
            s_transition_cb(AUTO_TRANSITION_OFF, 0);
        }
        ESP_LOGI(TAG, "Instant OFF");
        return;
    }

    s_fade_start_master  = from;
    s_fade_current_master = from;
    s_fade_target_master = 0;
    s_fade_duration_us   = (uint32_t)s_fade_out_s * 1000000UL;
    s_fade_start_us      = esp_timer_get_time();
    s_state              = AUTO_STATE_FADING_OUT;

    esp_timer_start_periodic(s_fade_timer, 50000);  /* 50 ms */
    ESP_LOGI(TAG, "Fade out: %u→0 over %us", from, s_fade_out_s);
}

/* ── Public API ── */

esp_err_t auto_mode_init(void)
{
    /* s_cfg populated via auto_mode_set_config() called from lamp_control_init */
    s_cfg.timeout_s     = AUTO_TIMEOUT_S_DEFAULT;
    s_cfg.lux_threshold = AUTO_LUX_DEFAULT;
    s_cfg.suppress_min  = AUTO_SUPPRESS_MIN_DEFAULT;
    lamp_nvs_load_active_scene(&s_active_scene);

    esp_timer_create_args_t timeout_args = {
        .callback = timeout_cb,
        .name     = "auto_timeout",
    };
    esp_timer_create(&timeout_args, &s_timeout_timer);

    esp_timer_create_args_t fade_args = {
        .callback          = fade_timer_cb,
        .dispatch_method   = ESP_TIMER_TASK,
        .name              = "auto_fade",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&fade_args, &s_fade_timer);

    esp_timer_create_args_t suppress_args = {
        .callback = suppress_cb,
        .name     = "auto_suppress",
    };
    esp_timer_create(&suppress_args, &s_suppress_timer);

    ESP_LOGI(TAG, "Auto mode initialised (timeout=%us, lux_thresh=%u, suppress=%umin)",
             s_cfg.timeout_s, s_cfg.lux_threshold, s_cfg.suppress_min);
    return ESP_OK;
}

void auto_mode_enable(void)
{
    auto_mode_cancel_suppress();
    s_enabled = true;
    s_state = AUTO_STATE_IDLE;
    s_fade_current_master = 0;
    ESP_LOGI(TAG, "Auto mode enabled");
}

void auto_mode_disable(void)
{
    s_enabled = false;
    esp_timer_stop(s_timeout_timer);
    esp_timer_stop(s_fade_timer);
    s_state = AUTO_STATE_IDLE;
    s_fade_current_master = 0;
    ESP_LOGI(TAG, "Auto mode disabled");
}

void auto_mode_process_event(const sensor_event_t *evt)
{
    if (!s_enabled) return;

    switch (evt->type) {
    case SENSOR_EVT_LUX_UPDATE:
        s_current_lux = evt->data.lux;
        break;

    case SENSOR_EVT_MOTION_START:
        if (s_state == AUTO_STATE_IDLE) {
            /* Only activate if dark enough */
            if (s_current_lux < s_cfg.lux_threshold) {
                ESP_LOGI(TAG, "Motion + dark (lux=%u) → fade in", s_current_lux);
                /* s_active_scene is always current via auto_mode_notify_scene_change() —
                 * no NVS load needed here. */
                s_fade_current_master = 0;
                start_fade_in(true);
            }
        } else if (s_state == AUTO_STATE_FADING_OUT) {
            /* Reverse: stop fade-out, fade back in from current brightness */
            ESP_LOGI(TAG, "Motion during fade-out → reverse to fade in (from master=%u)",
                     s_fade_current_master);
            esp_timer_stop(s_fade_timer);
            start_fade_in(false);
        } else if (s_state == AUTO_STATE_ON || s_state == AUTO_STATE_FADING_IN) {
            /* Motion while on or fading in — restart inactivity timeout */
            if (s_state == AUTO_STATE_ON) {
                esp_timer_stop(s_timeout_timer);
                esp_timer_start_once(s_timeout_timer,
                                     (uint64_t)s_cfg.timeout_s * 1000000);
            }
        }
        break;

    case SENSOR_EVT_MOTION_END:
        /* No immediate action — timeout timer handles the transition */
        break;

    default:
        break;
    }
}

auto_state_t auto_mode_get_state(void)
{
    return s_state;
}

esp_err_t auto_mode_set_config(const auto_config_t *cfg)
{
    s_cfg = *cfg;
    return ESP_OK;
}

void auto_mode_get_config(auto_config_t *cfg)
{
    *cfg = s_cfg;
}

void auto_mode_force_on(void)
{
    if (!s_enabled || s_state != AUTO_STATE_IDLE) return;
    s_fade_current_master = 0;
    start_fade_in(true);
    ESP_LOGI(TAG, "Force ON (group sync)");
}

void auto_mode_force_off(void)
{
    if (!s_enabled) return;
    if (s_state == AUTO_STATE_ON || s_state == AUTO_STATE_FADING_IN) {
        esp_timer_stop(s_timeout_timer);
        esp_timer_stop(s_fade_timer);
        start_fade_out();
        ESP_LOGI(TAG, "Force OFF (group sync)");
    }
}

void auto_mode_notify_scene_change(uint8_t warm, uint8_t neutral,
                                   uint8_t cool, uint8_t master)
{
    s_active_scene.warm    = warm;
    s_active_scene.neutral = neutral;
    s_active_scene.cool    = cool;
    s_active_scene.master  = master;

    switch (s_state) {
    case AUTO_STATE_FADING_IN:
        /* Redirect the in-progress fade to the new master target */
        s_fade_target_master = master;
        break;

    case AUTO_STATE_FADING_OUT:
        if (master > 0) {
            /* Sync says stay on — abort fade-out and return to ON state */
            esp_timer_stop(s_fade_timer);
            s_state = AUTO_STATE_ON;
            s_fade_current_master = master;
            /* Restart inactivity timer from now */
            esp_timer_stop(s_timeout_timer);
            esp_timer_start_once(s_timeout_timer,
                                 (uint64_t)s_cfg.timeout_s * 1000000);
            ESP_LOGI(TAG, "Sync override: abort fade-out → ON (master=%u)", master);
        }
        break;

    default:
        /* IDLE or ON: scene values updated; lamp_control handles LEDs directly */
        break;
    }
}

void auto_mode_suppress(uint16_t minutes, QueueHandle_t event_queue)
{
    auto_mode_disable();
    s_suppressed = true;
    s_suppress_queue = event_queue;
    esp_timer_stop(s_suppress_timer);
    uint64_t us = (uint64_t)minutes * 60ULL * 1000000ULL;
    esp_timer_start_once(s_suppress_timer, us);
    ESP_LOGI(TAG, "Auto mode suppressed for %u minutes", minutes);
}

void auto_mode_cancel_suppress(void)
{
    if (s_suppressed) {
        esp_timer_stop(s_suppress_timer);
        s_suppressed = false;
        ESP_LOGI(TAG, "Suppress cancelled");
    }
}

bool auto_mode_is_suppressed(void)
{
    return s_suppressed;
}
