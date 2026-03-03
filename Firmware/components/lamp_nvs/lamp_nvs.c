#include <stdio.h>
#include <string.h>
#include "lamp_nvs.h"
#include "sensor.h"
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

static void scene_set_new_field_defaults(scene_t *scene)
{
    scene->auto_timeout_s      = AUTO_TIMEOUT_S_DEFAULT;
    scene->auto_lux_threshold  = AUTO_LUX_DEFAULT;
    scene->flame_drift_x       = FLAME_DRIFT_X_DEFAULT;
    scene->flame_drift_y       = FLAME_DRIFT_Y_DEFAULT;
    scene->flame_restore       = FLAME_RESTORE_DEFAULT;
    scene->flame_radius        = FLAME_RADIUS_DEFAULT;
    scene->flame_bias_y        = FLAME_BIAS_Y_DEFAULT;
    scene->flame_flicker_depth = FLAME_FLICKER_DEPTH_DEFAULT;
    scene->flame_flicker_speed = FLAME_FLICKER_SPEED_DEFAULT;
    scene->flame_brightness    = FLAME_BRIGHTNESS_DEFAULT;
    scene->pir_sensitivity     = PIR_SENSITIVITY_DEFAULT;
}

esp_err_t lamp_nvs_load_active_scene(scene_t *scene)
{
    memset(scene, 0, sizeof(*scene));
    scene->fade_in_s  = FADE_IN_S_DEFAULT;
    scene->fade_out_s = FADE_OUT_S_DEFAULT;
    scene_set_new_field_defaults(scene);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    size_t len = sizeof(scene_t);
    esp_err_t ret = nvs_get_blob(h, "active", scene, &len);

    if (ret == ESP_ERR_NVS_INVALID_LENGTH) {
        /* Old blob smaller than new struct — re-read with actual stored size;
         * new fields remain at defaults set above */
        ret = nvs_get_blob(h, "active", scene, &len);
    }

    nvs_close(h);
    xSemaphoreGive(s_mutex);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        memset(scene, 0, sizeof(*scene));
        strncpy(scene->name, "Default", SCENE_NAME_MAX);
        scene->warm      = 200;
        scene->neutral   = 80;
        scene->cool      = 0;
        scene->master    = 128;
        scene->fade_in_s  = FADE_IN_S_DEFAULT;
        scene->fade_out_s = FADE_OUT_S_DEFAULT;
        scene_set_new_field_defaults(scene);
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

    memset(scene, 0, sizeof(*scene));
    scene->fade_in_s  = FADE_IN_S_DEFAULT;
    scene->fade_out_s = FADE_OUT_S_DEFAULT;
    scene_set_new_field_defaults(scene);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    size_t len = sizeof(scene_t);
    esp_err_t ret = nvs_get_blob(h, key, scene, &len);

    if (ret == ESP_ERR_NVS_INVALID_LENGTH) {
        /* Old blob smaller than new struct — re-read with actual stored size;
         * new fields remain at defaults set above */
        ret = nvs_get_blob(h, key, scene, &len);
    }

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

/* ── Sync group ── */

esp_err_t lamp_nvs_save_sync_group(uint8_t group_id)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_set_u8(h, "sync_grp", group_id);
    close_nvs(h);
    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t lamp_nvs_load_sync_group(uint8_t *group_id)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_get_u8(h, "sync_grp", group_id);
    nvs_close(h);
    xSemaphoreGive(s_mutex);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *group_id = 0;  /* disabled */
        return ESP_OK;
    }
    return ret;
}

/* ── Lamp name ── */

esp_err_t lamp_nvs_save_lamp_name(const char *name)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_set_str(h, "lamp_name", name);
    close_nvs(h);
    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t lamp_nvs_load_lamp_name(char *name, size_t max_len)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nvs_handle_t h = open_nvs();
    esp_err_t ret = nvs_get_str(h, "lamp_name", name, &max_len);
    nvs_close(h);
    xSemaphoreGive(s_mutex);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        name[0] = '\0';  /* empty = no custom name */
        return ESP_OK;
    }
    return ret;
}
