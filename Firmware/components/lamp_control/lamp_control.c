#include "lamp_control.h"
#include "led_driver.h"
#include "sensor.h"
#include "lamp_nvs.h"
#include "auto_mode.h"
#include "flame_mode.h"
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
static QueueHandle_t    s_sensor_queue;
static scene_t          s_active_scene;

/* ── Auto mode transition callback ── */

static void auto_transition_handler(auto_transition_t transition, uint8_t dim_master)
{
    switch (transition) {
    case AUTO_TRANSITION_ON:
        if (s_flags & MODE_FLAG_FLAME) {
            flame_mode_set_color(s_active_scene.warm, s_active_scene.neutral,
                                 s_active_scene.cool);
            if (!flame_mode_is_active()) {
                flame_mode_start();
            }
            flame_mode_set_master_override(255);
        } else {
            lamp_fill(s_active_scene.warm, s_active_scene.neutral, s_active_scene.cool);
            lamp_set_master(s_active_scene.master);
            lamp_flush();
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
        if (s_flags & MODE_FLAG_FLAME) {
            flame_mode_stop();
        }
        lamp_off();
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
        /* Give flame task time to self-delete */
        vTaskDelay(pdMS_TO_TICKS(100));
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
}

void lamp_control_apply_scene(const scene_t *scene)
{
    s_active_scene = *scene;
    lamp_nvs_save_active_scene(scene);

    if (s_flags & MODE_FLAG_FLAME) {
        flame_mode_set_color(scene->warm, scene->neutral, scene->cool);
    }

    if (s_flags == 0) {
        /* Pure manual: apply directly */
        apply_manual_scene();
    }
    /* If auto-only or auto+flame: scene stored for next auto ON transition */
}

void lamp_control_update_auto_config(const auto_config_t *cfg)
{
    auto_mode_set_config(cfg);
}

/* ── Event loop task ── */

static void control_task(void *arg)
{
    sensor_event_t evt;

    for (;;) {
        if (xQueueReceive(s_sensor_queue, &evt, portMAX_DELAY) == pdTRUE) {
            switch (evt.type) {
            case SENSOR_EVT_TOUCH_SHORT:
                ESP_LOGI(TAG, "Touch: short tap");
                if (s_flags == 0) {
                    /* Toggle lamp on/off only in pure manual mode */
                    s_lamp_on = !s_lamp_on;
                    if (s_lamp_on) {
                        apply_manual_scene();
                    } else {
                        lamp_fill(0, 0, 0);
                        lamp_set_master(0);
                        lamp_flush();
                    }
                }
                break;

            case SENSOR_EVT_TOUCH_LONG:
                ESP_LOGI(TAG, "Touch: long press → start advertising");
                ble_start_advertising();
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

    /* Initialise sub-modules */
    auto_mode_init();
    auto_mode_set_transition_cb(auto_transition_handler);

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
