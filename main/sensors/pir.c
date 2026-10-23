#include "pir.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_state.h"

static const char *TAG = "pir";

#define PIR_POLL_MS      40
#define PIR_HIGH_COUNT   2
#define PIR_LOW_COUNT    8

esp_err_t pir_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io);
}

static void pir_task(void *arg)
{
    (void)arg;
    int high_n = 0;
    int low_n = 0;
    bool present = false;

    ESP_LOGI(TAG, "watching GPIO%d", (int)PIR_GPIO);

    while (true) {
        if (gpio_get_level(PIR_GPIO)) {
            low_n = 0;
            if (++high_n >= PIR_HIGH_COUNT && !present) {
                if (app_state_pir_blocked()) {
                    high_n = PIR_HIGH_COUNT;
                } else {
                    present = true;
                    app_state_set_person(true);
                }
            }
        } else {
            high_n = 0;
            if (++low_n >= PIR_LOW_COUNT && present) {
                present = false;
                app_state_set_person(false);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(PIR_POLL_MS));
    }
}

esp_err_t pir_start_task(void)
{
    BaseType_t ok = xTaskCreate(pir_task, "pir", 3072, NULL, 5, NULL);
    return (ok == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}
