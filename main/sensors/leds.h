#pragma once

#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Ten SK6812 LEDs on the M5GO base, data on GPIO15. */
#define PET_LED_COUNT  10
#define PET_LED_GPIO   GPIO_NUM_15

esp_err_t leds_init(void);

/**
 * PIR saw a person. Side LEDs turn green for one minute, then switch off
 * until the next call. Ignored while the green minute is still running.
 */
void leds_on_person(void);

#ifdef __cplusplus
}
#endif
