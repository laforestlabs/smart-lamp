#include "circadian_mode.h"
#include "lamp_nvs.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <sys/time.h>
#include <time.h>

/*
 * Declared extern to avoid circular CMake dependency
 * (lamp_control REQUIRES circadian_mode).
 */
extern void lamp_control_set_state(uint8_t warm, uint8_t neutral, uint8_t cool, uint8_t master);
extern uint8_t lamp_control_get_master(void);

static const char *TAG = "circadian";

#define CIRCADIAN_PERIOD_US  (60 * 1000000ULL)  /* 60 seconds */

static esp_timer_handle_t s_timer;
static bool               s_active;
static bool               s_time_valid;     /* true after first BLE time sync */

/* ── Keyframe table (matches Flutter CircadianCalculator) ──
 * Each entry: { fractional_hour * 100, warm, neutral, cool }
 * Using fixed-point hours (×100) to avoid float in the table. */
typedef struct {
    uint16_t hour_x100;
    uint8_t  warm;
    uint8_t  neutral;
    uint8_t  cool;
} keyframe_t;

static const keyframe_t KEYFRAMES[] = {
    {    0, 255,   0,   0 },  /*  0:00 */
    {  600, 255,   0,   0 },  /*  6:00 */
    {  700, 230,  80,   0 },  /*  7:00 */
    {  800, 180, 180,   0 },  /*  8:00 */
    {  900, 100, 220,  60 },  /*  9:00 */
    { 1000,  40, 200, 200 },  /* 10:00 */
    { 1200,  20, 180, 255 },  /* 12:00 */
    { 1400,  40, 200, 200 },  /* 14:00 */
    { 1500, 100, 220,  60 },  /* 15:00 */
    { 1600, 180, 180,   0 },  /* 16:00 */
    { 1700, 220,  80,   0 },  /* 17:00 */
    { 1800, 255,  30,   0 },  /* 18:00 */
    { 1900, 255,   0,   0 },  /* 19:00 */
    { 2400, 255,   0,   0 },  /* 24:00 */
};

#define NUM_KEYFRAMES (sizeof(KEYFRAMES) / sizeof(KEYFRAMES[0]))

static uint8_t lerp_u8(uint8_t a, uint8_t b, uint16_t t_x1000)
{
    int32_t result = (int32_t)a + ((int32_t)(b - a) * (int32_t)t_x1000) / 1000;
    if (result < 0) return 0;
    if (result > 255) return 255;
    return (uint8_t)result;
}

static void circadian_calculate(int hour, int minute, uint8_t *warm, uint8_t *neutral, uint8_t *cool)
{
    uint16_t now_x100 = (uint16_t)(hour * 100 + (minute * 100) / 60);

    for (int i = 0; i < (int)NUM_KEYFRAMES - 1; i++) {
        uint16_t h1 = KEYFRAMES[i].hour_x100;
        uint16_t h2 = KEYFRAMES[i + 1].hour_x100;
        if (now_x100 >= h1 && now_x100 <= h2) {
            uint16_t t_x1000 = (h2 == h1) ? 0 : (uint16_t)(((uint32_t)(now_x100 - h1) * 1000) / (h2 - h1));
            *warm    = lerp_u8(KEYFRAMES[i].warm,    KEYFRAMES[i + 1].warm,    t_x1000);
            *neutral = lerp_u8(KEYFRAMES[i].neutral, KEYFRAMES[i + 1].neutral, t_x1000);
            *cool    = lerp_u8(KEYFRAMES[i].cool,    KEYFRAMES[i + 1].cool,    t_x1000);
            return;
        }
    }
    /* Fallback */
    *warm = 255; *neutral = 0; *cool = 0;
}

static void circadian_timer_cb(void *arg)
{
    (void)arg;
    if (!s_time_valid) return;

    time_t now;
    time(&now);
    struct tm tm;
    localtime_r(&now, &tm);

    uint8_t warm, neutral, cool;
    circadian_calculate(tm.tm_hour, tm.tm_min, &warm, &neutral, &cool);

    ESP_LOGI(TAG, "Circadian update: %02d:%02d → [%d,%d,%d]",
             tm.tm_hour, tm.tm_min, warm, neutral, cool);

    uint8_t master = lamp_control_get_master();
    if (master == 0) return;  /* Don't override if lamp is off */

    lamp_control_set_state(warm, neutral, cool, master);
}

void circadian_mode_init(void)
{
    esp_timer_create_args_t args = {
        .callback = circadian_timer_cb,
        .name = "circadian",
    };
    esp_timer_create(&args, &s_timer);
}

void circadian_mode_enable(void)
{
    if (s_active) return;
    s_active = true;
    ESP_LOGI(TAG, "Circadian mode enabled (time_valid=%d)", s_time_valid);
    /* Fire immediately if time is valid, then every 60s */
    if (s_time_valid) {
        circadian_timer_cb(NULL);
    }
    esp_timer_start_periodic(s_timer, CIRCADIAN_PERIOD_US);
}

void circadian_mode_disable(void)
{
    if (!s_active) return;
    s_active = false;
    esp_timer_stop(s_timer);
    ESP_LOGI(TAG, "Circadian mode disabled");
}

bool circadian_mode_is_active(void)
{
    return s_active;
}

void circadian_mode_set_time(uint32_t unix_epoch)
{
    struct timeval tv = { .tv_sec = (time_t)unix_epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    s_time_valid = true;

    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    ESP_LOGI(TAG, "Time synced: %04d-%02d-%02d %02d:%02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    /* If circadian is active, apply immediately with new time */
    if (s_active) {
        circadian_timer_cb(NULL);
    }
}
