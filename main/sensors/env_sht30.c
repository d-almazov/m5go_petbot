#include "env_sht30.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp/m5stack_core.h"
#include "app_state.h"

static const char *TAG = "env_sht30";

/* SHT30 single-shot, high repeatability, clock stretching disabled */
static const uint8_t SHT30_CMD_MEASURE[] = {0x24, 0x00};

static i2c_master_dev_handle_t s_dev;

esp_err_t env_sht30_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "bsp i2c");

    i2c_master_bus_handle_t bus = NULL;
    ESP_RETURN_ON_ERROR(i2c_master_get_bus_handle(BSP_I2C_NUM, &bus), TAG, "get bus");

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ENV_SHT30_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_cfg, &s_dev), TAG, "add device");

    ESP_LOGI(TAG, "SHT30 ready @ 0x%02X", ENV_SHT30_ADDR);
    return ESP_OK;
}

static esp_err_t env_sht30_read(float *temp_c, float *hum_rh)
{
    uint8_t raw[6] = {0};

    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(s_dev, SHT30_CMD_MEASURE, sizeof(SHT30_CMD_MEASURE), 100),
        TAG, "measure cmd");

    /* High repeatability ~15 ms; wait with margin */
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_RETURN_ON_ERROR(
        i2c_master_receive(s_dev, raw, sizeof(raw), 100),
        TAG, "read");

    const uint16_t t_raw = ((uint16_t)raw[0] << 8) | raw[1];
    const uint16_t h_raw = ((uint16_t)raw[3] << 8) | raw[4];

    *temp_c = -45.0f + 175.0f * ((float)t_raw / 65535.0f);
    *hum_rh = 100.0f * ((float)h_raw / 65535.0f);
    if (*hum_rh > 100.0f) {
        *hum_rh = 100.0f;
    }
    if (*hum_rh < 0.0f) {
        *hum_rh = 0.0f;
    }
    return ESP_OK;
}

static void env_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "env task started");

    while (true) {
        float t = 0.0f;
        float h = 0.0f;
        if (env_sht30_read(&t, &h) == ESP_OK) {
            app_state_set_env(t, h);
            ESP_LOGD(TAG, "%.1f C  %.1f %%RH", t, h);
        } else {
            ESP_LOGW(TAG, "read failed (is ENV-III on Port A?)");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

esp_err_t env_sht30_start_task(void)
{
    BaseType_t ok = xTaskCreate(env_task, "env", 4096, NULL, 4, NULL);
    return (ok == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}
