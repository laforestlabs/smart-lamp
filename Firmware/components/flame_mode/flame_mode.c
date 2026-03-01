#include <math.h>
#include <string.h>
#include "flame_mode.h"
#include "led_driver.h"
#include "lamp_nvs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

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

static TaskHandle_t      s_task = NULL;
static volatile bool     s_running;
static SemaphoreHandle_t s_task_done;    /* signalled when flame task exits */
static flame_config_t    s_cfg;
static volatile uint8_t  s_master_override = 255;

/* Base colour values (0–255) — set from active scene */
static volatile uint8_t s_color_w = 255;
static volatile uint8_t s_color_n = 0;
static volatile uint8_t s_color_c = 0;

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

    int   diag_counter = 0;  /* diagnostic logging counter */

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
        /* Compute pixel values at full range; brightness applied via lamp_set_master
         * so that gamma correction sees full-range values (avoids dead zone at low brightness) */
        uint8_t master_out = (uint16_t)cfg.brightness * s_master_override / 255;

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

        /* Snapshot colour values (may be updated from another task) */
        uint8_t cw = s_color_w;
        uint8_t cn = s_color_n;
        uint8_t cc = s_color_c;

        for (int i = 0; i < LED_COUNT; i++) {
            float cx = (float)led_coords[i].col;
            float cy = (float)led_coords[i].row;
            float dx = cx - fx;
            float dy = cy - fy;
            float d2 = dx * dx + dy * dy;

            /* scale is 0.0–1.0: spatial Gaussian × temporal flicker */
            float scale = expf(-d2 / two_sigma_sq) * flicker;
            if (scale < 0.0f) scale = 0.0f;
            if (scale > 1.0f) scale = 1.0f;

            uint8_t w = (uint8_t)((float)cw * scale);
            uint8_t n = (uint8_t)((float)cn * scale);
            uint8_t c = (uint8_t)((float)cc * scale);

            lamp_set_pixel(i, w, n, c);
        }

        lamp_set_master(master_out);   /* brightness applied after gamma */
        lamp_flush();

        /* Periodic diagnostic dump every 5 seconds */
        if (++diag_counter >= FLAME_FPS * 5) {
            diag_counter = 0;
            ESP_LOGI(TAG, "DIAG: color=[%d,%d,%d] master=%d pos=(%.1f,%.1f)",
                     cw, cn, cc, master_out, fx, fy);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(FLAME_PERIOD_MS));
    }

    lamp_off();
    s_task = NULL;
    xSemaphoreGive(s_task_done);
    vTaskDelete(NULL);
}

/* ── Public API ── */

esp_err_t flame_mode_start(void)
{
    if (s_running) return ESP_OK;

    /* Create the semaphore once (lazy init) */
    if (!s_task_done) {
        s_task_done = xSemaphoreCreateBinary();
        assert(s_task_done);
    }

    /* If a previous task is still winding down, wait for it */
    if (s_task != NULL) {
        ESP_LOGW(TAG, "Waiting for previous flame task to exit");
        xSemaphoreTake(s_task_done, pdMS_TO_TICKS(500));
    }

    lamp_nvs_load_flame_config(&s_cfg);
    s_master_override = 255;

    ESP_LOGI(TAG, "Starting flame: color=[%d,%d,%d] override=%d br=%d",
             s_color_w, s_color_n, s_color_c, s_master_override, s_cfg.brightness);

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
    ESP_LOGI(TAG, "Flame mode stopping");

    /* Wait for the task to actually exit before returning */
    if (s_task != NULL && s_task_done) {
        xSemaphoreTake(s_task_done, pdMS_TO_TICKS(500));
    }
}

esp_err_t flame_mode_set_config(const flame_config_t *cfg)
{
    ESP_LOGI(TAG, "set_config: dx=%d dy=%d rst=%d r=%d by=%d fd=%d fs=%d br=%d",
             cfg->drift_x, cfg->drift_y, cfg->restore, cfg->radius,
             cfg->bias_y, cfg->flicker_depth, cfg->flicker_speed, cfg->brightness);
    s_cfg = *cfg;
    return lamp_nvs_save_flame_config(cfg);
}

void flame_mode_get_config(flame_config_t *cfg)
{
    *cfg = s_cfg;
}

void flame_mode_set_color(uint8_t warm, uint8_t neutral, uint8_t cool)
{
    s_color_w = warm;
    s_color_n = neutral;
    s_color_c = cool;
    ESP_LOGI(TAG, "set_color: [%d,%d,%d]", warm, neutral, cool);
}

bool flame_mode_is_active(void)
{
    return s_running;
}

void flame_mode_set_master_override(uint8_t master)
{
    ESP_LOGI(TAG, "master_override: %d", master);
    s_master_override = master;
}
