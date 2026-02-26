#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "esp_check.h"

#include "led_driver.h"
#include "led_encoder.h"
#include "led_gamma.h"

static const char *TAG = "led_drv";

/* RMT resolution: 10 MHz (100 ns per tick) */
#define RMT_RESOLUTION_HZ   10000000

/* Internal state */
static led_pixel_t        s_framebuf[LED_COUNT];
static uint8_t            s_master = 255;
static SemaphoreHandle_t  s_mutex;
static rmt_channel_handle_t s_rmt_chan;
static rmt_encoder_handle_t s_encoder;

/* TX buffer: 3 bytes per LED [cool, warm, neutral] — SK6812WWA 24-bit protocol */
static uint8_t s_tx_buf[LED_COUNT * 3];

esp_err_t led_driver_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_mutex, ESP_ERR_NO_MEM, TAG, "mutex create failed");

    /* Configure RMT TX channel */
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num           = LED_GPIO,
        .clk_src            = RMT_CLK_SRC_DEFAULT,
        .resolution_hz      = RMT_RESOLUTION_HZ,
        .mem_block_symbols   = 64,
        .trans_queue_depth   = 4,
    };
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&tx_cfg, &s_rmt_chan), TAG, "RMT TX init failed");
    ESP_RETURN_ON_ERROR(rmt_enable(s_rmt_chan), TAG, "RMT enable failed");

    /* Create the SK6812 encoder */
    ESP_RETURN_ON_ERROR(sk6812_encoder_new(&s_encoder), TAG, "encoder create failed");

    /* Start with all LEDs off */
    memset(s_framebuf, 0, sizeof(s_framebuf));

    ESP_LOGI(TAG, "LED driver initialised: %d LEDs on GPIO %d", LED_COUNT, LED_GPIO);
    return ESP_OK;
}

void lamp_set_pixel(uint8_t index, uint8_t warm, uint8_t neutral, uint8_t cool)
{
    if (index >= LED_COUNT) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_framebuf[index].warm    = warm;
    s_framebuf[index].neutral = neutral;
    s_framebuf[index].cool    = cool;
    xSemaphoreGive(s_mutex);
}

void lamp_fill(uint8_t warm, uint8_t neutral, uint8_t cool)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < LED_COUNT; i++) {
        s_framebuf[i].warm    = warm;
        s_framebuf[i].neutral = neutral;
        s_framebuf[i].cool    = cool;
    }
    xSemaphoreGive(s_mutex);
}

void lamp_set_master(uint8_t brightness)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_master = brightness;
    xSemaphoreGive(s_mutex);
}

uint8_t lamp_get_master(void)
{
    return s_master;
}

void lamp_get_pixel(uint8_t index, led_pixel_t *out)
{
    if (index >= LED_COUNT || !out) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_framebuf[index];
    xSemaphoreGive(s_mutex);
}

void lamp_flush(void)
{
    /* Copy framebuffer and apply master brightness + gamma under lock */
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint8_t master = s_master;
    for (int i = 0; i < LED_COUNT; i++) {
        /* Gamma correct first, then scale by master — avoids crushing
         * low values into the gamma dead zone at low brightness */
        uint8_t w = (uint16_t)gamma_correct(s_framebuf[i].warm)    * master / 255;
        uint8_t n = (uint16_t)gamma_correct(s_framebuf[i].neutral) * master / 255;
        uint8_t c = (uint16_t)gamma_correct(s_framebuf[i].cool)    * master / 255;

        /* SK6812WWA byte order: [cool, warm, neutral] */
        s_tx_buf[i * 3 + 0] = c;
        s_tx_buf[i * 3 + 1] = w;
        s_tx_buf[i * 3 + 2] = n;
    }
    xSemaphoreGive(s_mutex);

    /* Transmit (blocks until previous TX completes) */
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    rmt_transmit(s_rmt_chan, s_encoder, s_tx_buf, sizeof(s_tx_buf), &tx_config);
    rmt_tx_wait_all_done(s_rmt_chan, portMAX_DELAY);
}

void lamp_off(void)
{
    lamp_fill(0, 0, 0);
    lamp_flush();
}
