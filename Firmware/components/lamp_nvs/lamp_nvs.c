#include <stdio.h>
#include <string.h>
#include "lamp_nvs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "lamp_nvs";
static const char *NVS_NAMESPACE = "lamp";
static SemaphoreHandle_t s_mutex;

/* ── Helpers ── */

static nvs_handle_t open_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return 0;
    return h;
}

static void close_nvs(nvs_handle_t h)
{
    nvs_commit(h);
    nvs_close(h);
}

static void make_key(char *buf, const char *prefix, uint8_t index)
{
    snprintf(buf, 16, "%s_%02u", prefix, index);
}

/* ── Init ── */

esp_err_t lamp_nvs_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated or version mismatch — erasing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_LOGI(TAG, "NVS initialised");
    return ret;
}

/* ── Active state ── */

esp_err_t lamp_nvs_save_active_scene(const scene_t *scene)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_set_blob(h, "active", scene, sizeof(scene_t));
    close_nvs(h);
    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t lamp_nvs_load_active_scene(scene_t *scene)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    size_t len = sizeof(scene_t);
    esp_err_t ret = nvs_get_blob(h, "active", scene, &len);
    nvs_close(h);
    xSemaphoreGive(s_mutex);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        /* Default: warm white at 50% */
        memset(scene, 0, sizeof(*scene));
        strncpy(scene->name, "Default", SCENE_NAME_MAX);
        scene->warm   = 200;
        scene->neutral = 80;
        scene->cool   = 0;
        scene->master = 128;
        return ESP_OK;
    }
    return ret;
}

esp_err_t lamp_nvs_save_mode(uint8_t mode)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_set_u8(h, "mode", mode);
    close_nvs(h);
    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t lamp_nvs_load_mode(uint8_t *mode)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_get_u8(h, "mode", mode);
    nvs_close(h);
    xSemaphoreGive(s_mutex);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *mode = 0;  /* no flags = manual */
        return ESP_OK;
    }
    return ret;
}

/* ── Auto config ── */

esp_err_t lamp_nvs_save_auto_config(const auto_config_t *cfg)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_set_blob(h, "auto_cfg", cfg, sizeof(auto_config_t));
    close_nvs(h);
    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t lamp_nvs_load_auto_config(auto_config_t *cfg)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    size_t len = sizeof(auto_config_t);
    esp_err_t ret = nvs_get_blob(h, "auto_cfg", cfg, &len);
    nvs_close(h);
    xSemaphoreGive(s_mutex);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        cfg->timeout_s      = 300;
        cfg->lux_threshold  = 50;
        cfg->dim_pct        = 30;
        cfg->dim_duration_s = 30;
        return ESP_OK;
    }
    return ret;
}

/* ── Scenes ── */

esp_err_t lamp_nvs_save_scene(uint8_t index, const scene_t *scene)
{
    if (index >= SCENE_MAX) return ESP_ERR_INVALID_ARG;

    char key[16];
    make_key(key, "scene", index);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_set_blob(h, key, scene, sizeof(scene_t));
    close_nvs(h);
    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t lamp_nvs_load_scene(uint8_t index, scene_t *scene)
{
    if (index >= SCENE_MAX) return ESP_ERR_INVALID_ARG;

    char key[16];
    make_key(key, "scene", index);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    size_t len = sizeof(scene_t);
    esp_err_t ret = nvs_get_blob(h, key, scene, &len);
    nvs_close(h);
    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t lamp_nvs_delete_scene(uint8_t index)
{
    if (index >= SCENE_MAX) return ESP_ERR_INVALID_ARG;

    char key[16];
    make_key(key, "scene", index);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_erase_key(h, key);
    close_nvs(h);
    xSemaphoreGive(s_mutex);

    if (ret == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    return ret;
}

uint8_t lamp_nvs_get_scene_count(void)
{
    uint8_t count = 0;
    scene_t tmp;
    for (uint8_t i = 0; i < SCENE_MAX; i++) {
        if (lamp_nvs_load_scene(i, &tmp) == ESP_OK) count++;
    }
    return count;
}

/* ── Schedules ── */

esp_err_t lamp_nvs_save_schedule(uint8_t index, const schedule_t *sched)
{
    if (index >= SCHEDULE_MAX) return ESP_ERR_INVALID_ARG;

    char key[16];
    make_key(key, "sched", index);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_set_blob(h, key, sched, sizeof(schedule_t));
    close_nvs(h);
    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t lamp_nvs_load_schedule(uint8_t index, schedule_t *sched)
{
    if (index >= SCHEDULE_MAX) return ESP_ERR_INVALID_ARG;

    char key[16];
    make_key(key, "sched", index);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    size_t len = sizeof(schedule_t);
    esp_err_t ret = nvs_get_blob(h, key, sched, &len);
    nvs_close(h);
    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t lamp_nvs_delete_schedule(uint8_t index)
{
    if (index >= SCHEDULE_MAX) return ESP_ERR_INVALID_ARG;

    char key[16];
    make_key(key, "sched", index);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_erase_key(h, key);
    close_nvs(h);
    xSemaphoreGive(s_mutex);

    if (ret == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    return ret;
}

uint8_t lamp_nvs_get_schedule_count(void)
{
    uint8_t count = 0;
    schedule_t tmp;
    for (uint8_t i = 0; i < SCHEDULE_MAX; i++) {
        if (lamp_nvs_load_schedule(i, &tmp) == ESP_OK) count++;
    }
    return count;
}

/* ── Flame config ── */

esp_err_t lamp_nvs_save_flame_config(const flame_config_t *cfg)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_set_blob(h, "flame_cfg", cfg, sizeof(flame_config_t));
    close_nvs(h);
    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t lamp_nvs_load_flame_config(flame_config_t *cfg)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    size_t len = sizeof(flame_config_t);
    esp_err_t ret = nvs_get_blob(h, "flame_cfg", cfg, &len);
    nvs_close(h);
    xSemaphoreGive(s_mutex);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        /* Defaults from spec §3.7 — scaled 0–255 */
        cfg->drift_x       = 64;    /* 0.25 * 255 */
        cfg->drift_y        = 51;    /* 0.20 * 255 */
        cfg->restore        = 20;    /* 0.08 * 255 */
        cfg->radius         = 91;    /* 1.4 / 4.0 * 255 ≈ 89 (scaled to max radius ~4) */
        cfg->bias_y         = 128;   /* 3.0 / 6.0 * 255 ≈ 128 — grid centre */
        cfg->flicker_depth  = 38;    /* 0.15 * 255 */
        cfg->flicker_speed  = 128;   /* mid-range */
        cfg->brightness     = 200;
        return ESP_OK;
    }
    return ret;
}
