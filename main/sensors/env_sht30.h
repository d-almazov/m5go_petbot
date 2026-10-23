#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Unit ENV-III SHT30 on Port A I2C (addr 0x44). */
#define ENV_SHT30_ADDR  0x44

esp_err_t env_sht30_init(void);

/** Start periodic read task (updates app_state). */
esp_err_t env_sht30_start_task(void);

#ifdef __cplusplus
}
#endif
