#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Onboard MPU6886, same I2C bus as Port A (addr 0x68). */
#define IMU_MPU6886_ADDR  0x68

esp_err_t imu_init(void);
esp_err_t imu_start_task(void);

#ifdef __cplusplus
}
#endif
