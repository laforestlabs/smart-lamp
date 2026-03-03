#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LAMP_NAME_MAX   32
#define SCENE_NAME_MAX  16
#define SCENE_MAX       16
#define SCHEDULE_MAX    16

/* Default fade durations (seconds) */
#define FADE_IN_S_DEFAULT   3
#define FADE_OUT_S_DEFAULT  10

/* Default auto-mode settings */
#define AUTO_TIMEOUT_S_DEFAULT  300
#define AUTO_LUX_DEFAULT        50

/* Default flame settings (match Flutter FlameConfig defaults) */
#define FLAME_DRIFT_X_DEFAULT        128
#define FLAME_DRIFT_Y_DEFAULT        102
#define FLAME_RESTORE_DEFAULT        20
#define FLAME_RADIUS_DEFAULT         128
#define FLAME_BIAS_Y_DEFAULT         128
#define FLAME_FLICKER_DEPTH_DEFAULT  13
#define FLAME_FLICKER_SPEED_DEFAULT  13
#define FLAME_BRIGHTNESS_DEFAULT     255
#define PIR_SENSITIVITY_DEFAULT      24

typedef struct {
    char     name[SCENE_NAME_MAX + 1];
    uint8_t  warm;
    uint8_t  neutral;
    uint8_t  cool;
    uint8_t  master;
    uint8_t  mode_flags;
    uint8_t  fade_in_s;          /* seconds to fade from off to full brightness */
    uint8_t  fade_out_s;         /* seconds to fade from full brightness to off */
    uint16_t auto_timeout_s;     /* inactivity timeout before fade-out */
    uint16_t auto_lux_threshold; /* don't activate above this lux level */
    uint8_t  flame_drift_x;
    uint8_t  flame_drift_y;
    uint8_t  flame_restore;
    uint8_t  flame_radius;
    uint8_t  flame_bias_y;
    uint8_t  flame_flicker_depth;
    uint8_t  flame_flicker_speed;
    uint8_t  flame_brightness;
    uint8_t  pir_sensitivity;    /* 0–31 */
} scene_t;

typedef struct {
    uint16_t timeout_s;
    uint16_t lux_threshold;
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

/* Lamp mode flags (bitmask) */
#define MODE_FLAG_AUTO   (1 << 0)   /* 0x01 */
#define MODE_FLAG_FLAME  (1 << 1)   /* 0x02 */
#define MODE_FLAGS_MASK  (MODE_FLAG_AUTO | MODE_FLAG_FLAME)

/**
 * Initialise NVS flash.  Must be called before any other lamp_nvs function.
 */
esp_err_t lamp_nvs_init(void);

/* ── Active state ── */
esp_err_t lamp_nvs_save_active_scene(const scene_t *scene);
esp_err_t lamp_nvs_load_active_scene(scene_t *scene);
esp_err_t lamp_nvs_save_mode(uint8_t mode);
esp_err_t lamp_nvs_load_mode(uint8_t *mode);

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

/* ── Sync group ── */
esp_err_t lamp_nvs_save_sync_group(uint8_t group_id);
esp_err_t lamp_nvs_load_sync_group(uint8_t *group_id);

/* ── Lamp name ── */
esp_err_t lamp_nvs_save_lamp_name(const char *name);
esp_err_t lamp_nvs_load_lamp_name(char *name, size_t max_len);

#ifdef __cplusplus
}
#endif
