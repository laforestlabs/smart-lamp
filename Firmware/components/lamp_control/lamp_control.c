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

static uint8_t          s_mode = MODE_MANUAL;
static bool             s_lamp_on = true;
static QueueHandle_t    s_sensor_queue;
static scene_t          s_active_scene;

/* ── Mode transition helpers ── */

static void stop_current_mode(void)
{
    switch (s_mode) {
    case MODE_AUTO:
        auto_mode_disable();
        break;
    case MODE_FLAME:
        flame_mode_stop();
        /* Give flame task time to self-delete */
        vTaskDelay(pdMS_TO_TICKS(100));
        break;
    default:
        break;
    }
}

static void start_mode(uint8_t mode)
{
    switch (mode) {
    case MODE_MANUAL:
        /* Restore active scene */
        lamp_fill(s_active_scene.warm, s_active_scene.neutral, s_active_scene.cool);
        lamp_set_master(s_active_scene.master);
        lamp_flush();
        break;
    case MODE_AUTO:
        auto_mode_enable();
        break;
    case MODE_FLAME:
        flame_mode_start();
        break;
    }
}

/* ── Public API ── */

uint8_t lamp_control_get_mode(void)
{
    return s_mode;
}

void lamp_control_set_mode(uint8_t mode)
{
    if (mode == s_mode) return;
    if (mode > MODE_FLAME) return;

    ESP_LOGI(TAG, "Mode change: %u → %u", s_mode, mode);

    stop_current_mode();
    s_mode = mode;
    lamp_nvs_save_mode(mode);
    start_mode(mode);
}

void lamp_control_apply_scene(const scene_t *scene)
{
    s_active_scene = *scene;
    lamp_nvs_save_active_scene(scene);

    if (s_mode == MODE_MANUAL) {
        lamp_fill(scene->warm, scene->neutral, scene->cool);
        lamp_set_master(scene->master);
        lamp_flush();
    }
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
                if (s_mode == MODE_MANUAL) {
                    /* Toggle lamp on/off */
                    s_lamp_on = !s_lamp_on;
                    if (s_lamp_on) {
                        lamp_fill(s_active_scene.warm, s_active_scene.neutral,
                                  s_active_scene.cool);
                        lamp_set_master(s_active_scene.master);
                    } else {
                        lamp_fill(0, 0, 0);
                        lamp_set_master(0);
                    }
                    lamp_flush();
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
                if (s_mode == MODE_AUTO) {
                    auto_mode_process_event(&evt);
                }

                /* Periodically notify BLE clients of sensor data */
                if (evt.type == SENSOR_EVT_MOTION_START ||
                    evt.type == SENSOR_EVT_MOTION_END) {
                    ble_notify_sensor_data();
                }
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
    lamp_nvs_load_mode(&s_mode);

    /* Initialise sub-modules */
    auto_mode_init();

    /* Apply saved mode */
    s_lamp_on = true;
    start_mode(s_mode);

    /* Create the event-loop task */
    BaseType_t ret = xTaskCreatePinnedToCore(control_task, "lamp_ctrl",
                                              CTRL_TASK_STACK, NULL,
                                              CTRL_TASK_PRIO, NULL, 0);
    if (ret != pdPASS) return ESP_ERR_NO_MEM;

    ESP_LOGI(TAG, "Lamp controller started (mode=%u, scene='%s')",
             s_mode, s_active_scene.name);
    return ESP_OK;
}
