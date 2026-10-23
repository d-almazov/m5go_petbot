#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "bsp/m5stack_core.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "app_state.h"
#include "beep.h"
#include "env_sht30.h"
#include "imu.h"
#include "leds.h"
#include "pir.h"
#include "ui.h"

static const char *TAG = "petbot";

#define BTN_A_GPIO          BSP_BUTTON_LEFT    /* GPIO39 — leftmost / first button */
#define BTN_B_GPIO          BSP_BUTTON_MIDDLE  /* GPIO38 — second button */
#define BTN_C_GPIO          BSP_BUTTON_RIGHT   /* GPIO37 — third button */
#define BTN_DEBOUNCE_MS     40
#define BTN_POLL_MS         20

/**
 * Approximate wall clock from firmware build time so the clock screen
 * looks useful offline. Replace later with SNTP when Wi-Fi is added.
 */
static void set_time_from_build(void)
{
    /* Example: "Oct 23 2026" + "19:05:01" */
    const char *d = __DATE__;
    const char *t = __TIME__;
    char mon_s[4] = {0};
    int day = 0;
    int year = 0;
    int hour = 0;
    int min = 0;
    int sec = 0;

    if (sscanf(d, "%3s %d %d", mon_s, &day, &year) != 3) {
        return;
    }
    if (sscanf(t, "%d:%d:%d", &hour, &min, &sec) != 3) {
        return;
    }

    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *p = strstr(months, mon_s);
    if (p == NULL) {
        return;
    }
    const int month = (int)((p - months) / 3) + 1;

    struct tm tm = {
        .tm_year = year - 1900,
        .tm_mon = month - 1,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = min,
        .tm_sec = sec,
        .tm_isdst = -1,
    };

    /* Ukraine / Eastern Europe summer-friendly default; adjust as needed */
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
    tzset();

    const time_t epoch = mktime(&tm);
    if (epoch > 0) {
        struct timeval tv = {.tv_sec = epoch, .tv_usec = 0};
        settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "clock seeded from build time");
    }
}

typedef struct {
    gpio_num_t gpio;
    bool stable;
    int debounce;
    void (*on_press)(void);
} btn_t;

static void buttons_task(void *arg)
{
    (void)arg;

    /* GPIO37–39 are input-only. External pull-ups, active low. No internal pulls. */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << BTN_A_GPIO) | (1ULL << BTN_B_GPIO) | (1ULL << BTN_C_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    btn_t btns[] = {
        {BTN_A_GPIO, false, 0, app_state_toggle_clock},
        {BTN_B_GPIO, false, 0, app_state_toggle_timer},
        {BTN_C_GPIO, false, 0, app_state_timer_start_stop},
    };

    ESP_LOGI(TAG, "Buttons A/B/C on GPIO%d/%d/%d",
             (int)BTN_A_GPIO, (int)BTN_B_GPIO, (int)BTN_C_GPIO);

    while (true) {
        for (size_t i = 0; i < sizeof(btns) / sizeof(btns[0]); i++) {
            const bool pressed = (gpio_get_level(btns[i].gpio) == 0);
            if (pressed == btns[i].stable) {
                btns[i].debounce = 0;
                continue;
            }
            btns[i].debounce += BTN_POLL_MS;
            if (btns[i].debounce >= BTN_DEBOUNCE_MS) {
                btns[i].stable = pressed;
                btns[i].debounce = 0;
                if (pressed) {
                    btns[i].on_press();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(BTN_POLL_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "M5GO Petbot starting (ESP-IDF FreeRTOS)");

    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);

    set_time_from_build();
    app_state_init();

    ESP_ERROR_CHECK(beep_init());
    ESP_ERROR_CHECK(ui_start());

    if (leds_init() != ESP_OK) {
        ESP_LOGW(TAG, "LED bar init failed — face still works");
    }

    esp_err_t imu_err = imu_init();
    if (imu_err != ESP_OK) {
        ESP_LOGW(TAG, "IMU init failed (%s) — screen stays as held at boot",
                 esp_err_to_name(imu_err));
    } else {
        ESP_ERROR_CHECK(imu_start_task());
    }

    /* ENV-III on Port A, PIR on Port B */
    esp_err_t env_err = env_sht30_init();
    if (env_err != ESP_OK) {
        ESP_LOGW(TAG, "ENV-III init failed (%s) — clock will show placeholders",
                 esp_err_to_name(env_err));
    } else {
        ESP_ERROR_CHECK(env_sht30_start_task());
    }

    ESP_ERROR_CHECK(pir_init());
    ESP_ERROR_CHECK(pir_start_task());

    xTaskCreate(buttons_task, "buttons", 3072, NULL, 6, NULL);

    ESP_LOGI(TAG, "ready — tilt turns the face, PIR says hello");
}
