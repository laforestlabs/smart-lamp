#include "led_encoder.h"
#include "esp_check.h"

static const char *TAG = "sk6812_enc";

/*
 * SK6812 NZR timing (in nanoseconds):
 *   T0H =  300 ns   T0L = 900 ns   (bit 0)
 *   T1H =  600 ns   T1L = 600 ns   (bit 1)
 *   Reset >= 80 us
 *
 * RMT resolution is set to 10 MHz (100 ns per tick).
 */

typedef struct {
    rmt_encoder_t           base;
    rmt_encoder_handle_t    bytes_encoder;
    rmt_encoder_handle_t    copy_encoder;
    int                     state;          /* 0 = encoding pixels, 1 = encoding reset */
    rmt_symbol_word_t       reset_code;
} sk6812_encoder_t;

static size_t sk6812_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                            const void *primary_data, size_t data_size,
                            rmt_encode_state_t *ret_state)
{
    sk6812_encoder_t *enc = __containerof(encoder, sk6812_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    switch (enc->state) {
    case 0: { /* encode pixel data as NZR bit stream */
        size_t n = enc->bytes_encoder->encode(enc->bytes_encoder, channel,
                                              primary_data, data_size,
                                              &session_state);
        encoded_symbols += n;
        if (session_state & RMT_ENCODING_COMPLETE) {
            enc->state = 1;     /* move to reset pulse */
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            *ret_state = (rmt_encode_state_t)RMT_ENCODING_MEM_FULL;
            return encoded_symbols;
        }
    }
    /* fall through */
    case 1: { /* encode the reset/latch pulse */
        size_t n = enc->copy_encoder->encode(enc->copy_encoder, channel,
                                             &enc->reset_code,
                                             sizeof(enc->reset_code),
                                             &session_state);
        encoded_symbols += n;
        if (session_state & RMT_ENCODING_COMPLETE) {
            enc->state = 0;     /* ready for next frame */
            *ret_state = (rmt_encode_state_t)RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            *ret_state = (rmt_encode_state_t)RMT_ENCODING_MEM_FULL;
        }
        return encoded_symbols;
    }
    }

    *ret_state = (rmt_encode_state_t)RMT_ENCODING_COMPLETE;
    return encoded_symbols;
}

static esp_err_t sk6812_reset(rmt_encoder_t *encoder)
{
    sk6812_encoder_t *enc = __containerof(encoder, sk6812_encoder_t, base);
    rmt_encoder_reset(enc->bytes_encoder);
    rmt_encoder_reset(enc->copy_encoder);
    enc->state = 0;
    return ESP_OK;
}

static esp_err_t sk6812_del(rmt_encoder_t *encoder)
{
    sk6812_encoder_t *enc = __containerof(encoder, sk6812_encoder_t, base);
    rmt_del_encoder(enc->bytes_encoder);
    rmt_del_encoder(enc->copy_encoder);
    free(enc);
    return ESP_OK;
}

esp_err_t sk6812_encoder_new(rmt_encoder_handle_t *ret_encoder)
{
    ESP_RETURN_ON_FALSE(ret_encoder, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    sk6812_encoder_t *enc = calloc(1, sizeof(sk6812_encoder_t));
    ESP_RETURN_ON_FALSE(enc, ESP_ERR_NO_MEM, TAG, "no memory for encoder");

    enc->base.encode = sk6812_encode;
    enc->base.reset  = sk6812_reset;
    enc->base.del    = sk6812_del;
    enc->state       = 0;

    /* Bytes encoder: converts each byte into 8 NZR bit waveforms.
     * Resolution = 10 MHz → 1 tick = 100 ns. */
    rmt_bytes_encoder_config_t bytes_cfg = {
        .bit0 = {
            .duration0 = 3,     /* T0H = 300 ns */
            .level0    = 1,
            .duration1 = 9,     /* T0L = 900 ns */
            .level1    = 0,
        },
        .bit1 = {
            .duration0 = 6,     /* T1H = 600 ns */
            .level0    = 1,
            .duration1 = 6,     /* T1L = 600 ns */
            .level1    = 0,
        },
        .flags.msb_first = 1,
    };
    esp_err_t ret = rmt_new_bytes_encoder(&bytes_cfg, &enc->bytes_encoder);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "create bytes encoder failed");

    /* Copy encoder: emits a single reset pulse (>= 80 us low). */
    rmt_copy_encoder_config_t copy_cfg = {};
    ret = rmt_new_copy_encoder(&copy_cfg, &enc->copy_encoder);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "create copy encoder failed");

    /* Reset pulse: 800 ticks × 100 ns = 80 us low */
    enc->reset_code = (rmt_symbol_word_t){
        .duration0 = 800,
        .level0    = 0,
        .duration1 = 0,
        .level1    = 0,
    };

    *ret_encoder = &enc->base;
    return ESP_OK;

err:
    if (enc->bytes_encoder) rmt_del_encoder(enc->bytes_encoder);
    if (enc->copy_encoder)  rmt_del_encoder(enc->copy_encoder);
    free(enc);
    return ret;
}
