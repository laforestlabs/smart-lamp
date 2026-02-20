#pragma once

#include <stdint.h>
#include "driver/rmt_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create an RMT encoder for SK6812WWA NZR protocol.
 *
 * Each LED consumes 4 bytes: [warm, neutral, cool, 0x00].
 * The encoder emits NZR bit-level waveforms followed by a >=80 us reset pulse.
 *
 * @param[out] ret_encoder  Pointer to receive the created encoder handle.
 * @return ESP_OK on success.
 */
esp_err_t sk6812_encoder_new(rmt_encoder_handle_t *ret_encoder);

#ifdef __cplusplus
}
#endif
