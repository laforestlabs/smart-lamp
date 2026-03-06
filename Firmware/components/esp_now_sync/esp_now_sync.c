#include "esp_now_sync.h"
#include "sensor.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_random.h"
#include "esp_coexist.h"  /* esp_coex_preference_set() */
#include "lamp_nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "esp_now_sync";

#define SYNC_MAGIC      0x4C    /* 'L' for Lamp */
#define SYNC_VERSION    0x02    /* v2: full scene + lamp_on */
#define MSG_STATE_SYNC  0x01

#define SYNC_TASK_STACK 3072
#define SYNC_TASK_PRIO  3

typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  version;
    uint8_t  group_id;
    uint8_t  msg_type;
    uint32_t sequence;
    /* Scene settings */
    uint8_t  warm;
    uint8_t  neutral;
    uint8_t  cool;
    uint8_t  master;             /* configured brightness target — never 0 to signal "off" */
    uint8_t  flags;
    uint8_t  fade_in_s;
    uint8_t  fade_out_s;
    uint16_t auto_timeout_s;
    uint16_t auto_lux_threshold;
    uint8_t  flame_config[8];   /* drift_x, drift_y, restore, radius, bias_y,
                                    flicker_depth, flicker_speed, brightness */
    uint8_t  pir_sensitivity;
    /* Operational state — decoupled from scene master */
    uint8_t  lamp_on;           /* 0 = off, 1 = on */
} sync_msg_t;                   /* 29 bytes */

static uint8_t       s_group_id = 0;
static uint32_t      s_seq = 0;
static uint8_t       s_own_mac[6];
static QueueHandle_t s_tx_queue;
static QueueHandle_t s_rx_queue;   /* sensor queue for deferred sync processing */

/* ── WiFi minimal init (STA mode, no AP connection) ── */

static esp_err_t wifi_init_sta_minimal(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    /* Event loop may already exist — ignore ESP_ERR_INVALID_STATE */
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Pin to channel 1 so all lamps are on the same channel for ESP-NOW */
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    /* Disable WiFi power save — keeps radio responsive for ESP-NOW */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    /* Max TX power (78 = 19.5 dBm) for best ESP-NOW range */
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(78));

    /* Prioritise WiFi for ESP-NOW delivery — BLE writes are infrequent so
     * slight BLE latency increase is acceptable for much better sync reliability */
    ESP_ERROR_CHECK(esp_coex_preference_set(ESP_COEX_PREFER_WIFI));

    esp_read_mac(s_own_mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "WiFi STA started (no AP) MAC=%02X:%02X:%02X:%02X:%02X:%02X",
             s_own_mac[0], s_own_mac[1], s_own_mac[2],
             s_own_mac[3], s_own_mac[4], s_own_mac[5]);
    return ESP_OK;
}

/* ── ESP-NOW send callback (runs in WiFi task context) ── */

static void esp_now_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "TX ack: OK");
    } else {
        ESP_LOGW(TAG, "TX ack: FAIL (no ACK from peer)");
    }
}

/* ── ESP-NOW receive callback (runs in WiFi task context) ── */

static void esp_now_recv_cb(const esp_now_recv_info_t *info,
                            const uint8_t *data, int len)
{
    if (len < (int)sizeof(sync_msg_t)) return;

    const sync_msg_t *msg = (const sync_msg_t *)data;

    if (msg->magic != SYNC_MAGIC || msg->version != SYNC_VERSION) return;
    if (memcmp(info->src_addr, s_own_mac, 6) == 0) return;
    if (msg->group_id != s_group_id || s_group_id == 0) return;

    if (msg->msg_type == MSG_STATE_SYNC) {
        ESP_LOGI(TAG, "RX from %02X:%02X seq=%lu [%d,%d,%d,%d] flags=0x%02x lamp_on=%d",
                 info->src_addr[4], info->src_addr[5],
                 (unsigned long)msg->sequence,
                 msg->warm, msg->neutral, msg->cool, msg->master,
                 msg->flags, msg->lamp_on);

        /* Post to lamp_control task via sensor queue — never block WiFi task */
        sensor_event_t evt = {
            .type = SENSOR_EVT_SYNC,
            .data.sync = {
                .warm             = msg->warm,
                .neutral          = msg->neutral,
                .cool             = msg->cool,
                .master           = msg->master,
                .flags            = msg->flags,
                .fade_in_s        = msg->fade_in_s,
                .fade_out_s       = msg->fade_out_s,
                .auto_timeout_s   = msg->auto_timeout_s,
                .auto_lux_threshold = msg->auto_lux_threshold,
                .pir_sensitivity  = msg->pir_sensitivity,
                .lamp_on          = msg->lamp_on,
            },
        };
        memcpy(evt.data.sync.flame_config, msg->flame_config, 8);
        xQueueSend(s_rx_queue, &evt, 0);  /* non-blocking */
    }
}

/* ── TX task ── */

static void sync_tx_task(void *arg)
{
    static const uint8_t broadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    sync_msg_t msg;

    for (;;) {
        if (xQueueReceive(s_tx_queue, &msg, portMAX_DELAY) == pdTRUE) {
            if (s_group_id == 0) continue;

            /* Send up to 12× with jittered gaps spread over ~2 s.
             * BLE coexistence blocks the radio during connection events (~30 ms
             * intervals on TX side) and advertising events (~100 ms on RX side).
             * Measured per-retry delivery is ~14% under BLE load.  Front-loading
             * the first 3 retries within ~55 ms improves best-case latency, and
             * wider jitter (0-79 ms) decorrelates retries from periodic BLE events.
             *
             * Between retries, check if a newer message is queued (from a rapid
             * state change). If so, abandon the current retries and immediately
             * switch to the newer message. */
            static const uint16_t base_gaps_ms[] = {5, 15, 35, 65, 100, 140, 170, 200, 250, 300, 350};
            for (int i = 0; i < 12; i++) {
                esp_err_t ret = esp_now_send(broadcast, (uint8_t *)&msg, sizeof(msg));
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "send[%d] enqueue failed: %s", i, esp_err_to_name(ret));
                } else {
                    ESP_LOGI(TAG, "send[%d] enqueued (seq=%lu)", i, (unsigned long)msg.sequence);
                }
                if (i < 11) {
                    uint32_t jitter = esp_random() % 80;
                    vTaskDelay(pdMS_TO_TICKS(base_gaps_ms[i] + jitter));
                    /* Check for newer message — supersedes current retries */
                    if (xQueueReceive(s_tx_queue, &msg, 0) == pdTRUE) {
                        ESP_LOGI(TAG, "newer state queued (seq=%lu), restarting retries",
                                 (unsigned long)msg.sequence);
                        i = -1;  /* restart retry loop with new message */
                    }
                }
            }
        }
    }
}

/* ── Public API ── */

esp_err_t esp_now_sync_init(QueueHandle_t sensor_queue)
{
    s_rx_queue = sensor_queue;
    lamp_nvs_load_sync_group(&s_group_id);

    ESP_ERROR_CHECK(wifi_init_sta_minimal());
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(esp_now_send_cb));

    /* Add broadcast peer */
    esp_now_peer_info_t peer = {
        .channel = 0,
        .ifidx   = WIFI_IF_STA,
        .encrypt = false,
    };
    memset(peer.peer_addr, 0xFF, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    /* Length-1 queue: xQueueOverwrite keeps only the latest state */
    s_tx_queue = xQueueCreate(1, sizeof(sync_msg_t));
    assert(s_tx_queue);

    xTaskCreatePinnedToCore(sync_tx_task, "esp_now_tx",
                            SYNC_TASK_STACK, NULL, SYNC_TASK_PRIO, NULL, 1);

    ESP_LOGI(TAG, "ESP-NOW sync init (group=%u)", s_group_id);
    return ESP_OK;
}

void esp_now_sync_broadcast(const scene_t *scene, bool lamp_on)
{
    if (s_group_id == 0) return;

    sync_msg_t msg = {
        .magic            = SYNC_MAGIC,
        .version          = SYNC_VERSION,
        .group_id         = s_group_id,
        .msg_type         = MSG_STATE_SYNC,
        .sequence         = ++s_seq,
        .warm             = scene->warm,
        .neutral          = scene->neutral,
        .cool             = scene->cool,
        .master           = scene->master,
        .flags            = scene->mode_flags,
        .fade_in_s        = scene->fade_in_s,
        .fade_out_s       = scene->fade_out_s,
        .auto_timeout_s   = scene->auto_timeout_s,
        .auto_lux_threshold = scene->auto_lux_threshold,
        .pir_sensitivity  = scene->pir_sensitivity,
        .lamp_on          = lamp_on ? 1 : 0,
    };
    msg.flame_config[0] = scene->flame_drift_x;
    msg.flame_config[1] = scene->flame_drift_y;
    msg.flame_config[2] = scene->flame_restore;
    msg.flame_config[3] = scene->flame_radius;
    msg.flame_config[4] = scene->flame_bias_y;
    msg.flame_config[5] = scene->flame_flicker_depth;
    msg.flame_config[6] = scene->flame_flicker_speed;
    msg.flame_config[7] = scene->flame_brightness;

    xQueueOverwrite(s_tx_queue, &msg);
}

esp_err_t esp_now_sync_set_group(uint8_t group_id)
{
    s_group_id = group_id;
    ESP_LOGI(TAG, "Group set to %u", group_id);
    return lamp_nvs_save_sync_group(group_id);
}

uint8_t esp_now_sync_get_group(void)
{
    return s_group_id;
}

void esp_now_sync_get_mac(uint8_t mac_out[6])
{
    memcpy(mac_out, s_own_mac, 6);
}
