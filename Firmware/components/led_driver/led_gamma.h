#pragma once

#include <stdint.h>

/**
 * Apply gamma 2.2 correction to an 8-bit value.
 */
uint8_t gamma_correct(uint8_t val);
