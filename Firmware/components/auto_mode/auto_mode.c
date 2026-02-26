#include "auto_mode.h"
#include "led_driver.h"
#include "lamp_nvs.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "auto_mode";

static auto_config_t   s_cfg;
static auto_state_t    s_state = AUTO_STATE_IDLE;
static bool            s_enabled = false;
static uint8_t         s_current_lux = 100;    /* assume bright until first reading */
static esp_timer_handle_t s_timeout_timer;
static esp_timer_handle_t s_dim_timer;

/* Saved scene to restore when entering ON state */
static scene_t s_active_scene;

/* Transition callback — set by lamp_control */
static auto_mode_transition_cb_t s_transition_cb = NULL;

void auto_mode_set_transition_cb(auto_mode_transition_cb_t cb)
{
    s_transition_cb = cb;
}

/* ── Timer callbacks ── */

static void timeout_cb(void *arg)
{
    /* No motion for timeout_s — enter DIMMING */
    if (s_state == AUTO_STATE_ON) {
        uint8_t dim_master = s_cfg.dim_pct * 255 / 100;
        ESP_LOGI(TAG, "Timeout → DIMMING (dim_pct=%u%%)", s_cfg.dim_pct);
        s_state = AUTO_STATE_DIMMING;

        if (s_transition_cb) {
            s_transition_cb(AUTO_TRANSITION_DIMMING, dim_master);
        } else {
            lamp_set_master(dim_master);
            lamp_flush();
        }

        /* Start dim duration timer */
        esp_timer_start_once(s_dim_timer, (uint64_t)s_cfg.dim_duration_s * 1000000);
    }
}

static void dim_cb(void *arg)
{
    /* Dim duration elapsed — go to IDLE (off) */
    if (s_state == AUTO_STATE_DIMMING) {
        ESP_LOGI(TAG, "Dim duration elapsed → IDLE");
        s_state = AUTO_STATE_IDLE;

        if (s_transition_cb) {
            s_transition_cb(AUTO_TRANSITION_OFF, 0);
        } else {
            lamp_off();
        }
    }
}

/* ── Public API ── */

esp_err_t auto_mode_init(void)
{
    lamp_nvs_load_auto_config(&s_cfg);
    lamp_nvs_load_active_scene(&s_active_scene);

    esp_timer_create_args_t timeout_args = {
        .callback = timeout_cb,
        .name     = "auto_timeout",
    };
    esp_timer_create(&timeout_args, &s_timeout_timer);

    esp_timer_create_args_t dim_args = {
        .callback = dim_cb,
        .name     = "auto_dim",
    };
    esp_timer_create(&dim_args, &s_dim_timer);

    ESP_LOGI(TAG, "Auto mode initialised (timeout=%us, lux_thresh=%u, dim=%u%%, dim_dur=%us)",
             s_cfg.timeout_s, s_cfg.lux_threshold, s_cfg.dim_pct, s_cfg.dim_duration_s);
    return ESP_OK;
}

void auto_mode_enable(void)
{
    s_enabled = true;
    s_state = AUTO_STATE_IDLE;
    ESP_LOGI(TAG, "Auto mode enabled");
}

void auto_mode_disable(void)
{
    s_enabled = false;
    esp_timer_stop(s_timeout_timer);
    esp_timer_stop(s_dim_timer);
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
                ESP_LOGI(TAG, "Motion + dark (lux=%u) → ON", s_current_lux);
                s_state = AUTO_STATE_ON;

                lamp_nvs_load_active_scene(&s_active_scene);

                if (s_transition_cb) {
                    s_transition_cb(AUTO_TRANSITION_ON, 0);
                } else {
                    lamp_fill(s_active_scene.warm, s_active_scene.neutral, s_active_scene.cool);
                    lamp_set_master(s_active_scene.master);
                    lamp_flush();
                }

                /* Start inactivity timeout */
                esp_timer_start_once(s_timeout_timer,
                                     (uint64_t)s_cfg.timeout_s * 1000000);
            }
        } else if (s_state == AUTO_STATE_DIMMING) {
            /* Motion during dimming → back to ON */
            ESP_LOGI(TAG, "Motion during DIMMING → ON");
            esp_timer_stop(s_dim_timer);
            s_state = AUTO_STATE_ON;

            if (s_transition_cb) {
                s_transition_cb(AUTO_TRANSITION_ON, 0);
            } else {
                lamp_set_master(s_active_scene.master);
                lamp_flush();
            }

            esp_timer_start_once(s_timeout_timer,
                                 (uint64_t)s_cfg.timeout_s * 1000000);
        } else if (s_state == AUTO_STATE_ON) {
            /* Motion while ON — restart timeout */
            esp_timer_stop(s_timeout_timer);
            esp_timer_start_once(s_timeout_timer,
                                 (uint64_t)s_cfg.timeout_s * 1000000);
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
    return lamp_nvs_save_auto_config(cfg);
}

void auto_mode_get_config(auto_config_t *cfg)
{
    *cfg = s_cfg;
}
