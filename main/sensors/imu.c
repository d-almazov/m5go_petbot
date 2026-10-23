#include "imu.h"

#include <math.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp/m5stack_core.h"
#include "app_state.h"

static const char *TAG = "imu";

#define MPU_REG_ACCEL     0x3B
#define MPU_REG_ACCEL_CFG 0x1C
#define MPU_REG_PWR1      0x6B
#define MPU_REG_WHO       0x75
#define MPU_WHO_AM_I      0x19

/* Held with the screen facing you and the buttons along the bottom,
 * +Y points up and +X points right. Flat on the table, |Z| wins and
 * the last upright rotation is kept.
 */
#define TILT_ON_G     0.55f
#define FLAT_G        0.82f

static i2c_master_dev_handle_t s_dev;

static esp_err_t mpu_write(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

static esp_err_t mpu_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, 100);
}

esp_err_t imu_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "bsp i2c");

    i2c_master_bus_handle_t bus = NULL;
    ESP_RETURN_ON_ERROR(i2c_master_get_bus_handle(BSP_I2C_NUM, &bus), TAG, "get bus");

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = IMU_MPU6886_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_cfg, &s_dev), TAG, "add device");

    ESP_RETURN_ON_ERROR(mpu_write(MPU_REG_PWR1, 0x80), TAG, "reset");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(mpu_write(MPU_REG_PWR1, 0x01), TAG, "wake");
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t who = 0;
    ESP_RETURN_ON_ERROR(mpu_read(MPU_REG_WHO, &who, 1), TAG, "whoami");
    if (who != MPU_WHO_AM_I) {
        ESP_LOGE(TAG, "unexpected WHO_AM_I 0x%02X", who);
        return ESP_ERR_NOT_FOUND;
    }

    /* ±2 g. 16384 LSB per g. */
    ESP_RETURN_ON_ERROR(mpu_write(MPU_REG_ACCEL_CFG, 0x00), TAG, "accel cfg");
    ESP_LOGI(TAG, "MPU6886 ready");
    return ESP_OK;
}

static int classify(float ax, float ay, float az)
{
    if (fabsf(az) > FLAT_G && fabsf(az) > fabsf(ax) && fabsf(az) > fabsf(ay)) {
        return -1;
    }
    if (fabsf(ay) >= fabsf(ax)) {
        if (ay > TILT_ON_G) {
            return APP_ROT_90;
        }
        if (ay < -TILT_ON_G) {
            return APP_ROT_270;
        }
    } else {
        if (ax > TILT_ON_G) {
            return APP_ROT_0;
        }
        if (ax < -TILT_ON_G) {
            return APP_ROT_180;
        }
    }
    return -1;
}

/** Which cube-timer face is down. Flat poses are faces too. */
static int classify_cube(float ax, float ay, float az)
{
    if (fabsf(az) > FLAT_G && fabsf(az) > fabsf(ax) && fabsf(az) > fabsf(ay)) {
        return (az > 0.0f) ? CUBE_FACE_1MIN : CUBE_FACE_60MIN;
    }
    if (fabsf(ay) >= fabsf(ax)) {
        if (ay > TILT_ON_G) {
            return CUBE_FACE_5MIN;
        }
        if (ay < -TILT_ON_G) {
            return CUBE_FACE_15MIN;
        }
    } else {
        if (ax > TILT_ON_G) {
            return CUBE_FACE_10MIN;
        }
        if (ax < -TILT_ON_G) {
            return CUBE_FACE_30MIN;
        }
    }
    return -1;
}

static void settle(int pose, int current, int *pending, int *stable, void (*apply)(int))
{
    if (pose < 0 || pose == current) {
        *pending = -1;
        *stable = 0;
    } else if (pose == *pending) {
        if (++(*stable) >= 3) {
            apply(pose);
            *stable = 0;
        }
    } else {
        *pending = pose;
        *stable = 1;
    }
}

static void apply_rotation(int pose)
{
    app_state_set_rotation((app_rotation_t)pose);
}

static void apply_cube(int pose)
{
    app_state_set_cube_face((cube_face_t)pose);
}

static void imu_task(void *arg)
{
    (void)arg;
    float fx = 0.0f;
    float fy = 0.0f;
    float fz = 0.0f;
    bool seeded = false;
    int rot_pending = -1;
    int rot_stable = 0;
    int cube_pending = -1;
    int cube_stable = 0;

    while (true) {
        uint8_t raw[6] = {0};
        if (mpu_read(MPU_REG_ACCEL, raw, sizeof(raw)) == ESP_OK) {
            const float ax = (int16_t)((raw[0] << 8) | raw[1]) / 16384.0f;
            const float ay = (int16_t)((raw[2] << 8) | raw[3]) / 16384.0f;
            const float az = (int16_t)((raw[4] << 8) | raw[5]) / 16384.0f;
            if (!seeded) {
                fx = ax;
                fy = ay;
                fz = az;
                seeded = true;
            } else {
                fx = fx * 0.75f + ax * 0.25f;
                fy = fy * 0.75f + ay * 0.25f;
                fz = fz * 0.75f + az * 0.25f;
            }

            settle(classify(fx, fy, fz), (int)app_state_get_rotation(),
                   &rot_pending, &rot_stable, apply_rotation);
            settle(classify_cube(fx, fy, fz), (int)app_state_cube_face(),
                   &cube_pending, &cube_stable, apply_cube);
        }
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}

esp_err_t imu_start_task(void)
{
    BaseType_t ok = xTaskCreate(imu_task, "imu", 3072, NULL, 4, NULL);
    return (ok == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}
