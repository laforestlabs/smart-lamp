#include <string.h>
#include "ble_service.h"
#include "ble_gatt.h"
#include "led_driver.h"
#include "sensor.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"

static const char *TAG = "ble_svc";

#define ADV_TIMEOUT_MS  180000  /* 3 min advertising timeout */

static uint16_t s_conn_handle;
static bool     s_connected = false;
static char     s_device_name[32];

/* ── Notification helpers ── */

static void notify_chr(uint16_t handle, const void *data, uint16_t len)
{
    if (!s_connected) return;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) return;

    int rc = ble_gatts_notify_custom(s_conn_handle, handle, om);
    if (rc != 0 && rc != BLE_HS_ENOTCONN) {
        ESP_LOGD(TAG, "Notify handle %u failed: %d", handle, rc);
    }
}

void ble_notify_led_state(void)
{
    /* Read current LED state and notify */
    led_pixel_t px;
    lamp_get_pixel(0, &px); /* representative pixel — for uniform fill this is sufficient */
    uint8_t buf[4] = { px.warm, px.neutral, px.cool, lamp_get_master() };
    notify_chr(g_led_state_handle, buf, sizeof(buf));
}

void ble_notify_sensor_data(void)
{
    uint16_t lux = sensor_get_lux();
    uint8_t motion = sensor_get_motion() ? 1 : 0;
    uint8_t buf[3];
    memcpy(buf, &lux, 2);
    buf[2] = motion;
    notify_chr(g_sensor_data_handle, buf, sizeof(buf));
}

void ble_notify_scene_list(void)
{
    /* Re-trigger a read via notification with empty payload to signal change */
    uint8_t dummy = 0;
    notify_chr(g_scene_list_handle, &dummy, 1);
}

void ble_notify_schedule_list(void)
{
    uint8_t dummy = 0;
    notify_chr(g_schedule_list_handle, &dummy, 1);
}

void ble_notify_ota_status(uint8_t status)
{
    notify_chr(g_ota_control_handle, &status, 1);
}

/* ── GAP event handler ── */

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            s_connected = true;
            ESP_LOGI(TAG, "Client connected (handle=%u)", s_conn_handle);

            /* Request MTU upgrade to 512 */
            ble_att_set_preferred_mtu(512);
            ble_gattc_exchange_mtu(s_conn_handle, NULL, NULL);
        } else {
            ESP_LOGW(TAG, "Connection failed: %d", event->connect.status);
            s_connected = false;
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Client disconnected (reason=%d)", event->disconnect.reason);
        s_connected = false;
        /* Restart advertising so the app can reconnect */
        ble_start_advertising();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Advertising complete (reason=%d)", event->adv_complete.reason);
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU updated: %u", event->mtu.value);
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        /* Delete old bond and allow re-pairing */
        struct ble_gap_conn_desc desc;
        ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        ble_store_util_delete_peer(&desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    default:
        break;
    }
    return 0;
}

/* ── Advertising ── */

void ble_start_advertising(void)
{
    struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };

    /* Advertising data: flags + 128-bit service UUID (fits in 31 bytes: 3+18=21).
     * The UUID must be in the ADV packet (not scan response) for iOS to filter on it. */
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    ble_uuid128_t svc_uuid = SVC_UUID_BASE;
    fields.uuids128 = &svc_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    /* Scan response: device name (16 bytes, fits easily) */
    struct ble_hs_adv_fields rsp_fields = {0};
    rsp_fields.name = (uint8_t *)s_device_name;
    rsp_fields.name_len = strlen(s_device_name);
    rsp_fields.name_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp_fields);

    int rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL,
                               ADV_TIMEOUT_MS,  /* duration in ms */
                               &adv_params, gap_event_handler, NULL);
    if (rc == 0) {
        ESP_LOGI(TAG, "Advertising started as '%s' (timeout=%ds)",
                 s_device_name, ADV_TIMEOUT_MS / 1000);
    } else {
        ESP_LOGE(TAG, "Advertising start failed: %d", rc);
    }
}

void ble_stop_advertising(void)
{
    ble_gap_adv_stop();
    ESP_LOGI(TAG, "Advertising stopped");
}

bool ble_is_connected(void)
{
    return s_connected;
}

/* ── NimBLE host sync callback ── */

static void ble_on_sync(void)
{
    /* Use serial number from NVS if set, otherwise fall back to MAC suffix */
    bool name_set = false;
    nvs_handle_t nvs;
    if (nvs_open("lamp", NVS_READONLY, &nvs) == ESP_OK) {
        char serial[12] = {0};
        size_t len = sizeof(serial);
        if (nvs_get_str(nvs, "serial_num", serial, &len) == ESP_OK && len > 1) {
            snprintf(s_device_name, sizeof(s_device_name), "SmartLamp-%s", serial);
            name_set = true;
        }
        nvs_close(nvs);
    }
    if (!name_set) {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_BT);
        snprintf(s_device_name, sizeof(s_device_name), "SmartLamp-%02X%02X",
                 mac[4], mac[5]);
    }

    ble_svc_gap_device_name_set(s_device_name);

    /* Ensure we have a public address */
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed: %d", rc);
    }

    /* Configure security: Just Works bonding */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;

    ESP_LOGI(TAG, "BLE host synced — device name: %s", s_device_name);

    /* Always advertise on boot so the app can reconnect.
     * TODO: revert to bond-check once touch button is available. */
    ESP_LOGI(TAG, "Starting advertising automatically");
    ble_start_advertising();
}

static void ble_on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset (reason=%d)", reason);
}

/* ── NimBLE host task ── */

static void nimble_host_task(void *arg)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ── Public init ── */

esp_err_t ble_init(void)
{
    int rc;

    /* Initialise NimBLE */
    rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
        return rc;
    }

    /* Configure NimBLE host callbacks */
    ble_hs_cfg.sync_cb  = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Register GATT services */
    rc = ble_gatt_register();
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT register failed: %d", rc);
        return ESP_FAIL;
    }

    /* NimBLE store is automatically initialised via NVS when
     * CONFIG_BT_NIMBLE_NVS_PERSIST is enabled in sdkconfig. */

    /* Start the NimBLE host task */
    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "BLE stack initialised");
    return ESP_OK;
}
