#include "leds.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "led_strip.h"

static const char *TAG = "leds";

#define LED_GREEN_US  (60LL * 1000 * 1000)

static led_strip_handle_t s_strip;
static SemaphoreHandle_t s_lock;
static esp_timer_handle_t s_off_timer;
static bool s_green;

static void fill(uint8_t r, uint8_t g, uint8_t b)
{
    if (s_strip == NULL) {
        return;
    }
    for (int i = 0; i < PET_LED_COUNT; i++) {
        led_strip_set_pixel(s_strip, i, r, g, b);
    }
    led_strip_refresh(s_strip);
}

static void green_off_cb(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_green = false;
    fill(0, 0, 0);
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "green minute ended");
}

esp_err_t leds_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = green_off_cb,
        .name = "led_green",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_off_timer), TAG, "timer");

    /* GPIO15 is a strap pin. Drive it only after boot. */
    const led_strip_config_t strip = {
        .strip_gpio_num = PET_LED_GPIO,
        .max_leds = PET_LED_COUNT,
        .led_model = LED_MODEL_SK6812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };
    const led_strip_rmt_config_t rmt = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        },
    };

    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip, &rmt, &s_strip), TAG, "strip");
    fill(0, 0, 0);
    ESP_LOGI(TAG, "base LEDs off until PIR");
    return ESP_OK;
}

void leds_on_person(void)
{
    if (s_lock == NULL || s_strip == NULL) {
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_green) {
        xSemaphoreGive(s_lock);
        return;
    }
    s_green = true;
    fill(0, 120, 0);
    xSemaphoreGive(s_lock);

    ESP_ERROR_CHECK(esp_timer_start_once(s_off_timer, LED_GREEN_US));
    ESP_LOGI(TAG, "green for 60 s");
}
