#include "lamp_control.h"
#include "led_driver.h"
#include "sensor.h"
#include "lamp_nvs.h"
#include "auto_mode.h"
#include "flame_mode.h"
#include "esp_now_sync.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
 * BLE functions are declared extern to avoid a circular CMake dependency
 * (ble_service REQUIRES lamp_control, so lamp_control cannot REQUIRE ble_service).
 * These are resolved at link time.
 */
extern void ble_start_advertising(void);
extern void ble_notify_sensor_data(void);

static const char *TAG = "lamp_ctrl";

#define CTRL_TASK_STACK     4096
#define CTRL_TASK_PRIO      5

static uint8_t          s_flags = 0;   /* bitmask: MODE_FLAG_AUTO | MODE_FLAG_FLAME */
static bool             s_lamp_on = true;
static bool             s_from_sync = false;  /* suppresses re-broadcast when true */
static uint8_t          s_configured_master = 128;  /* last non-zero master; used in broadcasts when lamp is off */
static QueueHandle_t    s_sensor_queue;
static scene_t          s_active_scene;

/* Forward declaration — defined after helpers */
static void broadcast_current_state(void);

/* ── Auto mode transition callback ── */

static void auto_transition_handler(auto_transition_t transition, uint8_t dim_master)
{
    switch (transition) {
    case AUTO_TRANSITION_ON:
        s_lamp_on = true;
        if (s_flags & MODE_FLAG_FLAME) {
            flame_mode_set_color(s_active_scene.warm, s_active_scene.neutral,
                                 s_active_scene.cool);
            if (!flame_mode_is_active()) {
                flame_mode_start();
            }
            /* dim_master=0 at fade-in start, full value when fade completes or instant */
            flame_mode_set_master_override(dim_master);
        } else {
            lamp_fill(s_active_scene.warm, s_active_scene.neutral, s_active_scene.cool);
            lamp_set_master(dim_master);
            lamp_flush();
        }
        /* Broadcast to group when lamp is fully on (not during the prep call with dim=0) */
        if (dim_master > 0) {
            broadcast_current_state();
        }
        break;

    case AUTO_TRANSITION_DIMMING:
        if (s_flags & MODE_FLAG_FLAME) {
            flame_mode_set_master_override(dim_master);
        } else {
            lamp_set_master(dim_master);
            lamp_flush();
        }
        break;

    case AUTO_TRANSITION_OFF:
        s_lamp_on = false;
        if (s_flags & MODE_FLAG_FLAME) {
            flame_mode_stop();
        }
        lamp_off();
        broadcast_current_state();
        break;
    }
}

/* ── Helpers ── */

static void apply_manual_scene(void)
{
    lamp_fill(s_active_scene.warm, s_active_scene.neutral, s_active_scene.cool);
    lamp_set_master(s_active_scene.master);
    lamp_flush();
}

/* Broadcast the current full scene + lamp_on state to group peers.
 * Suppressed when s_from_sync is set (prevents re-broadcast loops).
 * Always sends s_configured_master (last non-zero master) so peers never
 * receive master=0 as the scene target — lamp_on carries the on/off state. */
static void broadcast_current_state(void)
{
    if (s_from_sync) return;
    scene_t bc = s_active_scene;
    if (bc.master == 0) bc.master = s_configured_master;
    esp_now_sync_broadcast(&bc, s_lamp_on);
}

/* ── Public API ── */

uint8_t lamp_control_get_flags(void)
{
    return s_flags;
}

void lamp_control_set_flags(uint8_t flags)
{
    flags &= MODE_FLAGS_MASK;
    if (flags == s_flags) return;

    uint8_t old = s_flags;
    s_flags = flags;
    lamp_nvs_save_mode(flags);

    bool old_auto  = old & MODE_FLAG_AUTO;
    bool new_auto  = flags & MODE_FLAG_AUTO;
    bool old_flame = old & MODE_FLAG_FLAME;
    bool new_flame = flags & MODE_FLAG_FLAME;

    ESP_LOGI(TAG, "Flags change: 0x%02x → 0x%02x", old, flags);

    /* Stop what was running and is no longer needed */
    if (old_auto && !new_auto) {
        auto_mode_disable();
    }
    if (old_flame && !new_flame) {
        flame_mode_stop();
    }

    /* Start what is now needed */
    if (new_auto && !old_auto) {
        auto_mode_enable();
    }
    if (new_flame && !old_flame) {
        flame_mode_set_color(s_active_scene.warm, s_active_scene.neutral,
                             s_active_scene.cool);
        flame_mode_start();
    }

    /* If neither flag: restore manual static scene */
    if (flags == 0) {
        apply_manual_scene();
    }

    broadcast_current_state();
}

void lamp_control_apply_scene(const scene_t *scene)
{
    s_active_scene = *scene;
    lamp_nvs_save_active_scene(scene);

    /* Apply all per-scene config atomically */
    auto_config_t ac = { scene->auto_timeout_s, scene->auto_lux_threshold };
    auto_mode_set_config(&ac);
    auto_mode_set_fade_rates(scene->fade_in_s, scene->fade_out_s);

    flame_config_t fc = {
        scene->flame_drift_x, scene->flame_drift_y, scene->flame_restore,
        scene->flame_radius,  scene->flame_bias_y,  scene->flame_flicker_depth,
        scene->flame_flicker_speed, scene->flame_brightness,
    };
    flame_mode_set_config(&fc);

    sensor_set_pir_sensitivity(scene->pir_sensitivity);

    /* Restore mode flags stored with the scene */
    lamp_control_set_flags(scene->mode_flags);

    /* Keep auto_mode's internal scene current (colors + master fade target).
     * Must come after set_flags() in case auto_mode_enable() just reset state. */
    auto_mode_notify_scene_change(scene->warm, scene->neutral,
                                  scene->cool, scene->master);

    if (s_flags & MODE_FLAG_FLAME) {
        flame_mode_set_color(scene->warm, scene->neutral, scene->cool);
    }

    if (s_flags == 0) {
        /* Pure manual: apply directly */
        apply_manual_scene();
    }
    /* If auto-only or auto+flame: scene stored for next auto ON transition */
}

void lamp_control_set_state(uint8_t warm, uint8_t neutral, uint8_t cool, uint8_t master)
{
    s_active_scene.warm    = warm;
    s_active_scene.neutral = neutral;
    s_active_scene.cool    = cool;
    s_active_scene.master  = master;
    if (master > 0) s_configured_master = master;
    lamp_nvs_save_active_scene(&s_active_scene);

    ESP_LOGI(TAG, "set_state: [%d,%d,%d,%d] flags=0x%02x", warm, neutral, cool, master, s_flags);

    if (s_flags & MODE_FLAG_FLAME) {
        flame_mode_set_color(warm, neutral, cool);
        if (master == 0) {
            /* App on/off: stop flame and turn off LEDs */
            flame_mode_stop();
            lamp_off();
            s_lamp_on = false;
        } else if (!flame_mode_is_active()) {
            /* Turning back on: restart flame */
            flame_mode_start();
            s_lamp_on = true;
        }
    } else if (s_flags & MODE_FLAG_AUTO) {
        /* Keep auto_mode's internal scene in sync so fades target the right values.
         * This also aborts any in-progress fade-out if master > 0. */
        auto_mode_notify_scene_change(warm, neutral, cool, master);
        /* Auto-only mode: honour explicit on/off; apply colours when lamp is on */
        if (master == 0) {
            lamp_off();
            s_lamp_on = false;
        } else {
            apply_manual_scene();
            s_lamp_on = true;
        }
    } else {
        /* Manual mode: apply directly */
        apply_manual_scene();
    }

    broadcast_current_state();
}

void lamp_control_apply_sync(const sensor_sync_data_t *sync)
{
    /* Build a scene from the sync data, preserving the local name. */
    scene_t scene = s_active_scene;
    scene.warm               = sync->warm;
    scene.neutral            = sync->neutral;
    scene.cool               = sync->cool;
    scene.master             = sync->master;   /* always a configured target, never 0 */
    scene.mode_flags         = sync->flags;
    scene.fade_in_s          = sync->fade_in_s;
    scene.fade_out_s         = sync->fade_out_s;
    scene.auto_timeout_s     = sync->auto_timeout_s;
    scene.auto_lux_threshold = sync->auto_lux_threshold;
    scene.flame_drift_x      = sync->flame_config[0];
    scene.flame_drift_y      = sync->flame_config[1];
    scene.flame_restore      = sync->flame_config[2];
    scene.flame_radius       = sync->flame_config[3];
    scene.flame_bias_y       = sync->flame_config[4];
    scene.flame_flicker_depth  = sync->flame_config[5];
    scene.flame_flicker_speed  = sync->flame_config[6];
    scene.flame_brightness   = sync->flame_config[7];
    scene.pir_sensitivity    = sync->pir_sensitivity;

    /* Apply via the authoritative scene path: configures all sub-modules,
     * saves to NVS, and applies LEDs for manual mode.
     * s_from_sync=true suppresses re-broadcast throughout. */
    s_from_sync = true;
    lamp_control_apply_scene(&scene);
    /* apply_scene() calls auto_mode_notify_scene_change() internally — fade
     * targets are now up to date. Handle operational on/off separately. */
    if (s_flags & MODE_FLAG_AUTO) {
        if (sync->lamp_on && auto_mode_get_state() == AUTO_STATE_IDLE) {
            auto_mode_force_on();
        } else if (!sync->lamp_on && auto_mode_get_state() != AUTO_STATE_IDLE) {
            auto_mode_force_off();
        }
    }
    /* For manual/flame modes, apply_scene() already wrote the LEDs.
     * If the peer's lamp is off (lamp_on=0) in manual mode, turn off locally too. */
    if (!(s_flags & MODE_FLAG_AUTO) && !sync->lamp_on) {
        lamp_off();
    }
    s_lamp_on   = sync->lamp_on;
    s_configured_master = sync->master;  /* peer's configured master is our new target */
    s_from_sync = false;
}

void lamp_control_update_auto_config(const auto_config_t *cfg)
{
    auto_mode_set_config(cfg);
    s_active_scene.auto_timeout_s     = cfg->timeout_s;
    s_active_scene.auto_lux_threshold = cfg->lux_threshold;
    lamp_nvs_save_active_scene(&s_active_scene);
}

void lamp_control_update_flame_config(const flame_config_t *cfg)
{
    flame_mode_set_config(cfg);
    s_active_scene.flame_drift_x       = cfg->drift_x;
    s_active_scene.flame_drift_y       = cfg->drift_y;
    s_active_scene.flame_restore       = cfg->restore;
    s_active_scene.flame_radius        = cfg->radius;
    s_active_scene.flame_bias_y        = cfg->bias_y;
    s_active_scene.flame_flicker_depth = cfg->flicker_depth;
    s_active_scene.flame_flicker_speed = cfg->flicker_speed;
    s_active_scene.flame_brightness    = cfg->brightness;
    lamp_nvs_save_active_scene(&s_active_scene);
}

void lamp_control_set_pir_sensitivity(uint8_t level)
{
    sensor_set_pir_sensitivity(level);
    s_active_scene.pir_sensitivity = level;
    lamp_nvs_save_active_scene(&s_active_scene);
}

/* ── Event loop task ── */

static void control_task(void *arg)
{
    sensor_event_t evt;

    for (;;) {
        if (xQueueReceive(s_sensor_queue, &evt, portMAX_DELAY) == pdTRUE) {
            switch (evt.type) {
            case SENSOR_EVT_TOUCH_SHORT:
                ESP_LOGI(TAG, "Touch: short tap (on=%d, flags=0x%02x)", s_lamp_on, s_flags);
                if (s_lamp_on) {
                    /* Turn off: route through set_state so sync fires */
                    lamp_control_set_state(s_active_scene.warm, s_active_scene.neutral,
                                           s_active_scene.cool, 0);
                    if (s_flags & MODE_FLAG_AUTO) {
                        auto_mode_disable();
                    }
                    s_lamp_on = false;
                } else {
                    /* Turn on: route through set_state so sync fires */
                    lamp_control_set_state(s_active_scene.warm, s_active_scene.neutral,
                                           s_active_scene.cool, s_active_scene.master > 0
                                           ? s_active_scene.master : 128);
                    if (s_flags & MODE_FLAG_AUTO) {
                        auto_mode_enable();
                    }
                    s_lamp_on = true;
                }
                break;

            case SENSOR_EVT_TOUCH_LONG:
                ESP_LOGI(TAG, "Touch: long press → start advertising");
                ble_start_advertising();
                break;

            case SENSOR_EVT_SYNC:
                ESP_LOGI(TAG, "Sync RX: [%d,%d,%d,%d] flags=0x%02x lamp_on=%d",
                         evt.data.sync.warm, evt.data.sync.neutral,
                         evt.data.sync.cool, evt.data.sync.master,
                         evt.data.sync.flags, evt.data.sync.lamp_on);
                lamp_control_apply_sync(&evt.data.sync);
                break;

            case SENSOR_EVT_MOTION_START:
            case SENSOR_EVT_MOTION_END:
            case SENSOR_EVT_LUX_UPDATE:
                /* Forward to auto mode if active */
                if (s_flags & MODE_FLAG_AUTO) {
                    auto_mode_process_event(&evt);
                }

                /* Notify BLE clients of sensor data on any sensor event */
                ble_notify_sensor_data();
                break;
            }
        }
    }
}

/* ── Init ── */

esp_err_t lamp_control_init(QueueHandle_t sensor_queue)
{
    s_sensor_queue = sensor_queue;

    /* Load saved state */
    lamp_nvs_load_active_scene(&s_active_scene);
    lamp_nvs_load_mode(&s_flags);
    s_flags &= MODE_FLAGS_MASK;
    if (s_active_scene.master > 0) s_configured_master = s_active_scene.master;

    /* Apply all per-scene config from active scene */
    sensor_set_pir_sensitivity(s_active_scene.pir_sensitivity);

    auto_config_t ac = { s_active_scene.auto_timeout_s, s_active_scene.auto_lux_threshold };
    flame_config_t fc = {
        s_active_scene.flame_drift_x, s_active_scene.flame_drift_y,
        s_active_scene.flame_restore,  s_active_scene.flame_radius,
        s_active_scene.flame_bias_y,   s_active_scene.flame_flicker_depth,
        s_active_scene.flame_flicker_speed, s_active_scene.flame_brightness,
    };

    /* Initialise sub-modules */
    auto_mode_init();
    auto_mode_set_transition_cb(auto_transition_handler);
    auto_mode_set_config(&ac);
    auto_mode_set_fade_rates(s_active_scene.fade_in_s, s_active_scene.fade_out_s);
    flame_mode_set_config(&fc);

    /* Apply saved flags */
    s_lamp_on = true;
    if (s_flags & MODE_FLAG_AUTO) {
        auto_mode_enable();
    }
    if (s_flags & MODE_FLAG_FLAME) {
        flame_mode_set_color(s_active_scene.warm, s_active_scene.neutral,
                             s_active_scene.cool);
        flame_mode_start();
    }
    if (s_flags == 0) {
        apply_manual_scene();
    }

    /* Create the event-loop task */
    BaseType_t ret = xTaskCreatePinnedToCore(control_task, "lamp_ctrl",
                                              CTRL_TASK_STACK, NULL,
                                              CTRL_TASK_PRIO, NULL, 0);
    if (ret != pdPASS) return ESP_ERR_NO_MEM;

    ESP_LOGI(TAG, "Lamp controller started (flags=0x%02x, scene='%s')",
             s_flags, s_active_scene.name);
    return ESP_OK;
}
