#include "beep.h"

#include "bsp/m5stack_core.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "beep";
static volatile bool s_busy;

esp_err_t beep_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_speaker_init(), TAG, "speaker");
    return ESP_OK;
}

/* Minimal bit-bang square wave on the M5GO speaker GPIO. */
static void tone_ms(int freq_hz, int duration_ms)
{
    if (freq_hz <= 0) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        return;
    }
    const int half_us = 1000000 / (freq_hz * 2);
    const int64_t end = esp_timer_get_time() + (int64_t)duration_ms * 1000;
    while (esp_timer_get_time() < end) {
        gpio_set_level(BSP_SPEAKER_IO, 1);
        esp_rom_delay_us(half_us);
        gpio_set_level(BSP_SPEAKER_IO, 0);
        esp_rom_delay_us(half_us);
    }
}

static void beep_task(void *arg)
{
    (void)arg;
    tone_ms(880, 80);
    tone_ms(1175, 120);
    gpio_set_level(BSP_SPEAKER_IO, 0);
    s_busy = false;
    vTaskDelete(NULL);
}

static void alarm_task(void *arg)
{
    (void)arg;
    tone_ms(880, 140);
    tone_ms(698, 140);
    tone_ms(523, 220);
    gpio_set_level(BSP_SPEAKER_IO, 0);
    s_busy = false;
    vTaskDelete(NULL);
}

static void start_beep(TaskFunction_t fn, const char *name)
{
    if (s_busy) {
        return;
    }
    s_busy = true;
    if (xTaskCreate(fn, name, 2048, NULL, 3, NULL) != pdPASS) {
        s_busy = false;
        ESP_LOGW(TAG, "beep task failed");
    }
}

void beep_hello(void)
{
    start_beep(beep_task, "beep");
}

void beep_alarm(void)
{
    start_beep(alarm_task, "alarm");
}
