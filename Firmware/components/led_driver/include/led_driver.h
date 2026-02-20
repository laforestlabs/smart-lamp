#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LED_COUNT       31
#define LED_GPIO        19

typedef struct {
    uint8_t warm;
    uint8_t neutral;
    uint8_t cool;
} led_pixel_t;

typedef struct {
    uint8_t col;
    uint8_t row;
} led_coord_t;

/* Physical position of each LED (0-indexed, D1=index 0) */
extern const led_coord_t led_coords[LED_COUNT];

/**
 * Initialise the RMT TX channel for SK6812WWA on LED_GPIO.
 */
esp_err_t led_driver_init(void);

/**
 * Set a single pixel in the frame buffer (does not transmit).
 */
void lamp_set_pixel(uint8_t index, uint8_t warm, uint8_t neutral, uint8_t cool);

/**
 * Fill all pixels with the same colour (does not transmit).
 */
void lamp_fill(uint8_t warm, uint8_t neutral, uint8_t cool);

/**
 * Set the master brightness scaler (0–255).  Applied at flush time.
 */
void lamp_set_master(uint8_t brightness);

/**
 * Get the current master brightness.
 */
uint8_t lamp_get_master(void);

/**
 * Read back a pixel from the frame buffer.
 */
void lamp_get_pixel(uint8_t index, led_pixel_t *out);

/**
 * Apply master brightness + gamma correction and transmit the frame buffer
 * to the LED strip via RMT.
 */
void lamp_flush(void);

/**
 * Turn all LEDs off immediately (fill black + flush).
 */
void lamp_off(void);

#ifdef __cplusplus
}
#endif
