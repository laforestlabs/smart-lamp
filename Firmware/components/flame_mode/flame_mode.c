#include <math.h>
#include <string.h>
#include "flame_mode.h"
#include "led_driver.h"
#include "lamp_nvs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "flame";

#define FLAME_TASK_STACK    4096
#define FLAME_TASK_PRIO     4
#define FLAME_FPS           30
#define FLAME_PERIOD_MS     (1000 / FLAME_FPS)

/* ── Scaled config → float conversion helpers ── */
/* drift_x/y: 0–255 → 0.0–0.5 */
#define SCALE_DRIFT(v)      ((float)(v) / 255.0f * 0.5f)
/* restore: 0–255 → 0.0–0.3 */
#define SCALE_RESTORE(v)    ((float)(v) / 255.0f * 0.3f)
/* radius: 0–255 → 0.5–4.0 */
#define SCALE_RADIUS(v)     (0.5f + (float)(v) / 255.0f * 3.5f)
/* bias_y: 0–255 → 0.0–6.0 */
#define SCALE_BIAS_Y(v)     ((float)(v) / 255.0f * 6.0f)
/* flicker_depth: 0–255 → 0.0–1.0 */
#define SCALE_FLICKER_D(v)  ((float)(v) / 255.0f)
/* flicker_speed: 0–255 → 1.0–10.0 Hz */
#define SCALE_FLICKER_S(v)  (1.0f + (float)(v) / 255.0f * 9.0f)

static TaskHandle_t     s_task;
static volatile bool    s_running;
static flame_config_t   s_cfg;

/* ── Gaussian random (Box-Muller) ── */

static float randf_uniform(void)
{
    /* esp_random() returns uint32_t, uniform over full range */
    return (float)(esp_random() >> 8) / (float)(1 << 24);
}

static float randf_gaussian(float mean, float stddev)
{
    float u1 = randf_uniform();
    float u2 = randf_uniform();
    if (u1 < 1e-10f) u1 = 1e-10f;
    float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * M_PI * u2);
    return mean + stddev * z;
}

/* ── Flame task ── */

static void flame_task(void *arg)
{
    /* Flame centre starts at grid centre */
    float fx = 2.0f;
    float fy = 2.5f;

    /* Flicker phase */
    float flicker_phase = 0.0f;
    float flicker_phase_target = randf_uniform() * 2.0f * M_PI;
    int   flicker_phase_counter = 0;
    int   flicker_phase_interval = FLAME_FPS;   /* re-randomise every ~1s */

    TickType_t last_wake = xTaskGetTickCount();

    while (s_running) {
        /* Read current config (may be updated via BLE) */
        flame_config_t cfg = s_cfg;

        float drift_x     = SCALE_DRIFT(cfg.drift_x);
        float drift_y     = SCALE_DRIFT(cfg.drift_y);
        float k_restore   = SCALE_RESTORE(cfg.restore);
        float sigma_flame = SCALE_RADIUS(cfg.radius);
        float bias_y      = SCALE_BIAS_Y(cfg.bias_y);
        float fl_depth    = SCALE_FLICKER_D(cfg.flicker_depth);
        float fl_speed    = SCALE_FLICKER_S(cfg.flicker_speed);
        float master      = (float)cfg.brightness;

        /* ── Random walk ── */
        fx += randf_gaussian(0.0f, drift_x) - k_restore * (fx - 2.0f);
        fy += randf_gaussian(0.0f, drift_y) - k_restore * (fy - bias_y);

        /* Clamp to populated region */
        if (fx < 1.0f) fx = 1.0f;
        if (fx > 3.0f) fx = 3.0f;
        if (fy < 0.5f) fy = 0.5f;
        if (fy > 5.5f) fy = 5.5f;

        /* ── Global flicker ── */
        float t = (float)xTaskGetTickCount() / (float)configTICK_RATE_HZ;
        float flicker = 1.0f - fl_depth * fabsf(sinf(t * fl_speed * 2.0f * M_PI + flicker_phase));

        /* Re-randomise flicker phase periodically */
        if (++flicker_phase_counter >= flicker_phase_interval) {
            flicker_phase_counter = 0;
            flicker_phase = flicker_phase_target;
            flicker_phase_target = randf_uniform() * 2.0f * M_PI;
            /* Next interval: 0.5–2.0 seconds */
            flicker_phase_interval = FLAME_FPS / 2 + (esp_random() % (FLAME_FPS * 2));
        }
        /* Smoothly interpolate toward target phase */
        flicker_phase += (flicker_phase_target - flicker_phase) * 0.05f;

        /* ── Per-LED intensity ── */
        float two_sigma_sq = 2.0f * sigma_flame * sigma_flame;

        for (int i = 0; i < LED_COUNT; i++) {
            float cx = (float)led_coords[i].col;
            float cy = (float)led_coords[i].row;
            float dx = cx - fx;
            float dy = cy - fy;
            float d2 = dx * dx + dy * dy;

            float intensity = master * expf(-d2 / two_sigma_sq) * flicker;
            if (intensity < 0.0f) intensity = 0.0f;
            if (intensity > 255.0f) intensity = 255.0f;

            uint8_t warm    = (uint8_t)intensity;
            uint8_t neutral = (uint8_t)(intensity * 0.35f);

            lamp_set_pixel(i, warm, neutral, 0);
        }

        lamp_set_master(255);   /* master already baked into intensity */
        lamp_flush();

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(FLAME_PERIOD_MS));
    }

    lamp_off();
    vTaskDelete(NULL);
}

/* ── Public API ── */

esp_err_t flame_mode_start(void)
{
    if (s_running) return ESP_OK;

    lamp_nvs_load_flame_config(&s_cfg);

    s_running = true;
    BaseType_t ret = xTaskCreatePinnedToCore(flame_task, "flame", FLAME_TASK_STACK,
                                              NULL, FLAME_TASK_PRIO, &s_task, 0);
    if (ret != pdPASS) {
        s_running = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Flame mode started (%d fps)", FLAME_FPS);
    return ESP_OK;
}

void flame_mode_stop(void)
{
    if (!s_running) return;
    s_running = false;
    /* Task will self-delete after loop exits */
    ESP_LOGI(TAG, "Flame mode stopping");
}

esp_err_t flame_mode_set_config(const flame_config_t *cfg)
{
    s_cfg = *cfg;
    return lamp_nvs_save_flame_config(cfg);
}

void flame_mode_get_config(flame_config_t *cfg)
{
    *cfg = s_cfg;
}

bool flame_mode_is_active(void)
{
    return s_running;
}
