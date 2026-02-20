#include "lamp_ota.h"
#include "esp_ota_ops.h"
#include "esp_log.h"

static const char *TAG = "lamp_ota";

static esp_ota_handle_t     s_ota_handle;
static const esp_partition_t *s_update_partition;
static bool                 s_in_progress = false;

void lamp_ota_check_rollback(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "First boot after OTA — marking app valid");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

    ESP_LOGI(TAG, "Running from partition '%s' at offset 0x%lx",
             running->label, (unsigned long)running->address);
}

esp_err_t lamp_ota_begin(void)
{
    if (s_in_progress) {
        ESP_LOGW(TAG, "OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    s_update_partition = esp_ota_get_next_update_partition(NULL);
    if (!s_update_partition) {
        ESP_LOGE(TAG, "No OTA partition available");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "OTA begin → partition '%s' at offset 0x%lx",
             s_update_partition->label, (unsigned long)s_update_partition->address);

    esp_err_t ret = esp_ota_begin(s_update_partition, OTA_WITH_SEQUENTIAL_WRITES, &s_ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_in_progress = true;
    return ESP_OK;
}

esp_err_t lamp_ota_write_chunk(const uint8_t *data, size_t len)
{
    if (!s_in_progress) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = esp_ota_write(s_ota_handle, data, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(ret));
        lamp_ota_abort();
    }
    return ret;
}

esp_err_t lamp_ota_finish(void)
{
    if (!s_in_progress) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = esp_ota_end(s_ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed (image invalid?): %s", esp_err_to_name(ret));
        s_in_progress = false;
        return ret;
    }

    ret = esp_ota_set_boot_partition(s_update_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(ret));
        s_in_progress = false;
        return ret;
    }

    ESP_LOGI(TAG, "OTA complete — reboot to activate new firmware");
    s_in_progress = false;
    return ESP_OK;
}

void lamp_ota_abort(void)
{
    if (s_in_progress) {
        esp_ota_abort(s_ota_handle);
        s_in_progress = false;
        ESP_LOGW(TAG, "OTA aborted");
    }
}

bool lamp_ota_in_progress(void)
{
    return s_in_progress;
}
