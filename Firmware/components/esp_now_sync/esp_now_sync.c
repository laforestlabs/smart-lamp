#include "esp_now_sync.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "lamp_nvs.h"
#include "lamp_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "esp_now_sync";

#define SYNC_MAGIC      0x4C    /* 'L' for Lamp */
#define SYNC_VERSION    0x01
#define MSG_STATE_SYNC  0x01

#define SYNC_TASK_STACK 3072
#define SYNC_TASK_PRIO  3

typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  version;
    uint8_t  group_id;
    uint8_t  msg_type;
    uint32_t sequence;
    uint8_t  warm;
    uint8_t  neutral;
    uint8_t  cool;
    uint8_t  master;
    uint8_t  flags;
} sync_msg_t;

static uint8_t       s_group_id = 0;
static uint32_t      s_seq = 0;
static uint8_t       s_own_mac[6];
static QueueHandle_t s_tx_queue;

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

    esp_read_mac(s_own_mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "WiFi STA started (no AP) MAC=%02X:%02X:%02X:%02X:%02X:%02X",
             s_own_mac[0], s_own_mac[1], s_own_mac[2],
             s_own_mac[3], s_own_mac[4], s_own_mac[5]);
    return ESP_OK;
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
        ESP_LOGI(TAG, "RX from %02X:%02X seq=%lu [%d,%d,%d,%d] flags=0x%02x",
                 info->src_addr[4], info->src_addr[5],
                 (unsigned long)msg->sequence,
                 msg->warm, msg->neutral, msg->cool, msg->master, msg->flags);

        /* Apply flags and state together to avoid intermediate states */
        lamp_control_apply_sync(msg->warm, msg->neutral, msg->cool,
                                msg->master, msg->flags);
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

            esp_err_t ret = esp_now_send(broadcast, (uint8_t *)&msg, sizeof(msg));
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "send failed: %s", esp_err_to_name(ret));
            }
        }
    }
}

/* ── Public API ── */

esp_err_t esp_now_sync_init(void)
{
    lamp_nvs_load_sync_group(&s_group_id);

    ESP_ERROR_CHECK(wifi_init_sta_minimal());
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));

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

void esp_now_sync_broadcast(uint8_t warm, uint8_t neutral, uint8_t cool,
                            uint8_t master, uint8_t flags)
{
    if (s_group_id == 0) return;

    sync_msg_t msg = {
        .magic    = SYNC_MAGIC,
        .version  = SYNC_VERSION,
        .group_id = s_group_id,
        .msg_type = MSG_STATE_SYNC,
        .sequence = ++s_seq,
        .warm     = warm,
        .neutral  = neutral,
        .cool     = cool,
        .master   = master,
        .flags    = flags,
    };

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
