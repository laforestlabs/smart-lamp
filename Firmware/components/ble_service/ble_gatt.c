#include <string.h>
#include "ble_gatt.h"
#include "ble_service.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_log.h"

#include "led_driver.h"
#include "lamp_nvs.h"
#include "lamp_ota.h"
#include "lamp_control.h"
#include "sensor.h"
#include "flame_mode.h"

static const char *TAG = "ble_gatt";

/* Value handles — set during GATT registration */
uint16_t g_led_state_handle;
uint16_t g_mode_handle;
uint16_t g_auto_config_handle;
uint16_t g_scene_write_handle;
uint16_t g_scene_list_handle;
uint16_t g_schedule_write_handle;
uint16_t g_schedule_list_handle;
uint16_t g_sensor_data_handle;
uint16_t g_ota_control_handle;
uint16_t g_ota_data_handle;
uint16_t g_flame_config_handle;
uint16_t g_device_info_handle;

/* Firmware version string */
#define FW_VERSION "1.0.0"

/* ═══════════════════════ Characteristic Callbacks ═══════════════════════ */

/* ── LED State (0001): R/W/N — [warm, neutral, cool, master] ── */

static int led_state_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        scene_t scene;
        lamp_nvs_load_active_scene(&scene);
        uint8_t buf[4] = { scene.warm, scene.neutral, scene.cool, scene.master };
        os_mbuf_append(ctxt->om, buf, sizeof(buf));
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len < 4) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

        uint8_t buf[4];
        os_mbuf_copydata(ctxt->om, 0, 4, buf);

        lamp_fill(buf[0], buf[1], buf[2]);
        lamp_set_master(buf[3]);
        lamp_flush();

        /* Save as active scene */
        scene_t scene;
        lamp_nvs_load_active_scene(&scene);
        scene.warm    = buf[0];
        scene.neutral = buf[1];
        scene.cool    = buf[2];
        scene.master  = buf[3];
        lamp_nvs_save_active_scene(&scene);

        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/* ── Mode (0002): R/W — [mode: u8] ── */

static int mode_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t mode = lamp_control_get_mode();
        os_mbuf_append(ctxt->om, &mode, 1);
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t mode;
        os_mbuf_copydata(ctxt->om, 0, 1, &mode);
        if (mode > MODE_FLAME) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        lamp_control_set_mode(mode);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/* ── Auto Config (0003): R/W — [timeout_s:u16, lux_threshold:u16, dim_pct:u8, dim_duration_s:u16] ── */

static int auto_config_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        auto_config_t cfg;
        lamp_nvs_load_auto_config(&cfg);
        uint8_t buf[7];
        memcpy(&buf[0], &cfg.timeout_s, 2);
        memcpy(&buf[2], &cfg.lux_threshold, 2);
        buf[4] = cfg.dim_pct;
        memcpy(&buf[5], &cfg.dim_duration_s, 2);
        os_mbuf_append(ctxt->om, buf, sizeof(buf));
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len < 7) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

        uint8_t buf[7];
        os_mbuf_copydata(ctxt->om, 0, 7, buf);

        auto_config_t cfg;
        memcpy(&cfg.timeout_s, &buf[0], 2);
        memcpy(&cfg.lux_threshold, &buf[2], 2);
        cfg.dim_pct = buf[4];
        memcpy(&cfg.dim_duration_s, &buf[5], 2);
        lamp_nvs_save_auto_config(&cfg);

        /* Live-update if auto mode is active */
        lamp_control_update_auto_config(&cfg);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/* ── Scene Write (0004): W — [index, name_len, name, warm, neutral, cool, master] ── */

static int scene_write_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;

    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < 6) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    uint8_t buf[64];
    uint16_t copy_len = len > sizeof(buf) ? sizeof(buf) : len;
    os_mbuf_copydata(ctxt->om, 0, copy_len, buf);

    uint8_t index    = buf[0];
    uint8_t name_len = buf[1];
    if (name_len > SCENE_NAME_MAX) name_len = SCENE_NAME_MAX;
    if (2 + name_len + 4 > copy_len) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    scene_t scene = {0};
    memcpy(scene.name, &buf[2], name_len);
    scene.name[name_len] = '\0';
    scene.warm    = buf[2 + name_len + 0];
    scene.neutral = buf[2 + name_len + 1];
    scene.cool    = buf[2 + name_len + 2];
    scene.master  = buf[2 + name_len + 3];

    lamp_nvs_save_scene(index, &scene);
    ble_notify_scene_list();

    ESP_LOGI(TAG, "Scene %u saved: '%s'", index, scene.name);
    return 0;
}

/* ── Scene List (0005): R/N ── */

static int scene_list_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_UNLIKELY;

    /* Format: [count, {index, name_len, name, w, n, c, m}, ...] */
    uint8_t count = lamp_nvs_get_scene_count();
    os_mbuf_append(ctxt->om, &count, 1);

    scene_t scene;
    for (uint8_t i = 0; i < SCENE_MAX; i++) {
        if (lamp_nvs_load_scene(i, &scene) != ESP_OK) continue;

        uint8_t name_len = strlen(scene.name);
        os_mbuf_append(ctxt->om, &i, 1);
        os_mbuf_append(ctxt->om, &name_len, 1);
        os_mbuf_append(ctxt->om, scene.name, name_len);
        os_mbuf_append(ctxt->om, &scene.warm, 1);
        os_mbuf_append(ctxt->om, &scene.neutral, 1);
        os_mbuf_append(ctxt->om, &scene.cool, 1);
        os_mbuf_append(ctxt->om, &scene.master, 1);
    }
    return 0;
}

/* ── Schedule Write (0006): W — [index, day_mask, hour, minute, scene_index, enabled] ── */

static int schedule_write_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;

    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < 6) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    uint8_t buf[6];
    os_mbuf_copydata(ctxt->om, 0, 6, buf);

    schedule_t sched = {
        .day_mask    = buf[1],
        .hour        = buf[2],
        .minute      = buf[3],
        .scene_index = buf[4],
        .enabled     = buf[5] != 0,
    };

    lamp_nvs_save_schedule(buf[0], &sched);
    ble_notify_schedule_list();

    ESP_LOGI(TAG, "Schedule %u saved", buf[0]);
    return 0;
}

/* ── Schedule List (0007): R/N ── */

static int schedule_list_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_UNLIKELY;

    uint8_t count = lamp_nvs_get_schedule_count();
    os_mbuf_append(ctxt->om, &count, 1);

    schedule_t sched;
    for (uint8_t i = 0; i < SCHEDULE_MAX; i++) {
        if (lamp_nvs_load_schedule(i, &sched) != ESP_OK) continue;

        uint8_t buf[6] = {
            i, sched.day_mask, sched.hour, sched.minute,
            sched.scene_index, sched.enabled ? 1 : 0,
        };
        os_mbuf_append(ctxt->om, buf, sizeof(buf));
    }
    return 0;
}

/* ── Sensor Data (0008): R/N — [lux:u16 LE, motion:u8] ── */

static int sensor_data_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_UNLIKELY;

    uint16_t lux = sensor_get_lux();
    uint8_t motion = sensor_get_motion() ? 1 : 0;
    uint8_t buf[3];
    memcpy(buf, &lux, 2);
    buf[2] = motion;
    os_mbuf_append(ctxt->om, buf, sizeof(buf));
    return 0;
}

/* ── OTA Control (0009): W/N ── */

static int ota_control_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;

    uint8_t cmd;
    os_mbuf_copydata(ctxt->om, 0, 1, &cmd);

    esp_err_t ret;
    switch (cmd) {
    case OTA_CMD_START:
        ret = lamp_ota_begin();
        ble_notify_ota_status(ret == ESP_OK ? OTA_STATUS_BUSY : OTA_STATUS_ERROR);
        break;
    case OTA_CMD_END:
        ret = lamp_ota_finish();
        ble_notify_ota_status(ret == ESP_OK ? OTA_STATUS_OK : OTA_STATUS_ERROR);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "OTA complete — rebooting in 1 s");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        }
        break;
    case OTA_CMD_ABORT:
        lamp_ota_abort();
        ble_notify_ota_status(OTA_STATUS_READY);
        break;
    default:
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    return 0;
}

/* ── OTA Data (000A): W-no-resp ── */

static int ota_data_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;

    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    uint8_t buf[512];
    uint16_t copy_len = len > sizeof(buf) ? sizeof(buf) : len;
    os_mbuf_copydata(ctxt->om, 0, copy_len, buf);

    esp_err_t ret = lamp_ota_write_chunk(buf, copy_len);
    if (ret != ESP_OK) {
        ble_notify_ota_status(OTA_STATUS_ERROR);
    }
    return 0;
}

/* ── Flame Config (000C): R/W ── */

static int flame_config_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        flame_config_t cfg;
        flame_mode_get_config(&cfg);
        os_mbuf_append(ctxt->om, &cfg, sizeof(cfg));
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len < sizeof(flame_config_t)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

        flame_config_t cfg;
        os_mbuf_copydata(ctxt->om, 0, sizeof(cfg), &cfg);
        flame_mode_set_config(&cfg);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/* ── Device Info (000D): R ── */

static int device_info_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_UNLIKELY;

    os_mbuf_append(ctxt->om, FW_VERSION, strlen(FW_VERSION));
    return 0;
}

/* ═══════════════════════ GATT Service Definition ═══════════════════════ */

static const ble_uuid128_t svc_uuid = SVC_UUID_BASE;

static const ble_uuid128_t chr_led_state_uuid       = CHR_UUID(0xAA, 0x01);
static const ble_uuid128_t chr_mode_uuid             = CHR_UUID(0xAA, 0x02);
static const ble_uuid128_t chr_auto_config_uuid      = CHR_UUID(0xAA, 0x03);
static const ble_uuid128_t chr_scene_write_uuid      = CHR_UUID(0xAA, 0x04);
static const ble_uuid128_t chr_scene_list_uuid       = CHR_UUID(0xAA, 0x05);
static const ble_uuid128_t chr_schedule_write_uuid   = CHR_UUID(0xAA, 0x06);
static const ble_uuid128_t chr_schedule_list_uuid    = CHR_UUID(0xAA, 0x07);
static const ble_uuid128_t chr_sensor_data_uuid      = CHR_UUID(0xAA, 0x08);
static const ble_uuid128_t chr_ota_control_uuid      = CHR_UUID(0xAA, 0x09);
static const ble_uuid128_t chr_ota_data_uuid         = CHR_UUID(0xAA, 0x0A);
static const ble_uuid128_t chr_flame_config_uuid     = CHR_UUID(0xAA, 0x0C);
static const ble_uuid128_t chr_device_info_uuid      = CHR_UUID(0xAA, 0x0D);

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            { /* LED State (0001) */
                .uuid       = &chr_led_state_uuid.u,
                .access_cb  = led_state_access,
                .val_handle = &g_led_state_handle,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
            },
            { /* Mode (0002) */
                .uuid       = &chr_mode_uuid.u,
                .access_cb  = mode_access,
                .val_handle = &g_mode_handle,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            { /* Auto Config (0003) */
                .uuid       = &chr_auto_config_uuid.u,
                .access_cb  = auto_config_access,
                .val_handle = &g_auto_config_handle,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            { /* Scene Write (0004) */
                .uuid       = &chr_scene_write_uuid.u,
                .access_cb  = scene_write_access,
                .val_handle = &g_scene_write_handle,
                .flags      = BLE_GATT_CHR_F_WRITE,
            },
            { /* Scene List (0005) */
                .uuid       = &chr_scene_list_uuid.u,
                .access_cb  = scene_list_access,
                .val_handle = &g_scene_list_handle,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            { /* Schedule Write (0006) */
                .uuid       = &chr_schedule_write_uuid.u,
                .access_cb  = schedule_write_access,
                .val_handle = &g_schedule_write_handle,
                .flags      = BLE_GATT_CHR_F_WRITE,
            },
            { /* Schedule List (0007) */
                .uuid       = &chr_schedule_list_uuid.u,
                .access_cb  = schedule_list_access,
                .val_handle = &g_schedule_list_handle,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            { /* Sensor Data (0008) */
                .uuid       = &chr_sensor_data_uuid.u,
                .access_cb  = sensor_data_access,
                .val_handle = &g_sensor_data_handle,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            { /* OTA Control (0009) */
                .uuid       = &chr_ota_control_uuid.u,
                .access_cb  = ota_control_access,
                .val_handle = &g_ota_control_handle,
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
            },
            { /* OTA Data (000A) */
                .uuid       = &chr_ota_data_uuid.u,
                .access_cb  = ota_data_access,
                .val_handle = &g_ota_data_handle,
                .flags      = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { /* Flame Config (000C) */
                .uuid       = &chr_flame_config_uuid.u,
                .access_cb  = flame_config_access,
                .val_handle = &g_flame_config_handle,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            { /* Device Info (000D) */
                .uuid       = &chr_device_info_uuid.u,
                .access_cb  = device_info_access,
                .val_handle = &g_device_info_handle,
                .flags      = BLE_GATT_CHR_F_READ,
            },
            { 0 }, /* terminator */
        },
    },
    { 0 }, /* terminator */
};

/* ═══════════════════════ Registration ═══════════════════════ */

int ble_gatt_register(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return rc;
    }

    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return rc;
    }

    ESP_LOGI(TAG, "GATT services registered");
    return 0;
}

const struct ble_gatt_svc_def *ble_gatt_get_svcs(void)
{
    return s_gatt_svcs;
}
