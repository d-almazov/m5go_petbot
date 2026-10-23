#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unit PIR on M5GO Port B.
 * Grove white wire is the digital output (GPIO36). Yellow (GPIO26) is unused.
 * High = motion detected. GPIO36 is input-only and has no internal pull.
 */
#define PIR_GPIO  GPIO_NUM_36

esp_err_t pir_init(void);

/** Start FreeRTOS task that posts person events into app_state. */
esp_err_t pir_start_task(void);

#ifdef __cplusplus
}
#endif
