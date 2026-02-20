#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCENE_NAME_MAX  16
#define SCENE_MAX       16
#define SCHEDULE_MAX    16

typedef struct {
    char    name[SCENE_NAME_MAX + 1];
    uint8_t warm;
    uint8_t neutral;
    uint8_t cool;
    uint8_t master;
} scene_t;

typedef struct {
    uint16_t timeout_s;
    uint16_t lux_threshold;
    uint8_t  dim_pct;
    uint16_t dim_duration_s;
} auto_config_t;

typedef struct {
    uint8_t day_mask;       /* bit 0 = Mon, bit 6 = Sun */
    uint8_t hour;
    uint8_t minute;
    uint8_t scene_index;    /* 0xFF = turn off */
    bool    enabled;
} schedule_t;

typedef struct {
    uint8_t drift_x;
    uint8_t drift_y;
    uint8_t restore;
    uint8_t radius;
    uint8_t bias_y;
    uint8_t flicker_depth;
    uint8_t flicker_speed;
    uint8_t brightness;
} flame_config_t;

/* Lamp modes */
#define MODE_MANUAL  0
#define MODE_AUTO    1
#define MODE_FLAME   2

/**
 * Initialise NVS flash.  Must be called before any other lamp_nvs function.
 */
esp_err_t lamp_nvs_init(void);

/* ── Active state ── */
esp_err_t lamp_nvs_save_active_scene(const scene_t *scene);
esp_err_t lamp_nvs_load_active_scene(scene_t *scene);
esp_err_t lamp_nvs_save_mode(uint8_t mode);
esp_err_t lamp_nvs_load_mode(uint8_t *mode);

/* ── Auto config ── */
esp_err_t lamp_nvs_save_auto_config(const auto_config_t *cfg);
esp_err_t lamp_nvs_load_auto_config(auto_config_t *cfg);

/* ── Scenes (up to SCENE_MAX) ── */
esp_err_t lamp_nvs_save_scene(uint8_t index, const scene_t *scene);
esp_err_t lamp_nvs_load_scene(uint8_t index, scene_t *scene);
esp_err_t lamp_nvs_delete_scene(uint8_t index);
uint8_t   lamp_nvs_get_scene_count(void);

/* ── Schedules (up to SCHEDULE_MAX) ── */
esp_err_t lamp_nvs_save_schedule(uint8_t index, const schedule_t *sched);
esp_err_t lamp_nvs_load_schedule(uint8_t index, schedule_t *sched);
esp_err_t lamp_nvs_delete_schedule(uint8_t index);
uint8_t   lamp_nvs_get_schedule_count(void);

/* ── Flame config ── */
esp_err_t lamp_nvs_save_flame_config(const flame_config_t *cfg);
esp_err_t lamp_nvs_load_flame_config(flame_config_t *cfg);

#ifdef __cplusplus
}
#endif
