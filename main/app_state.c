#include "app_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "beep.h"
#include "leds.h"

static const char *TAG = "app_state";

static SemaphoreHandle_t s_lock;
static app_screen_t s_screen = APP_SCREEN_PET;
static pet_mood_t s_mood = PET_MOOD_CALM;
static app_rotation_t s_rot = APP_ROT_90;
static env_reading_t s_env;
static bool s_person;
static bool s_screen_dirty = true;
static bool s_env_dirty;
static bool s_rot_dirty = true;
static cube_face_t s_cube = CUBE_FACE_5MIN;
static int s_remaining_sec = 5 * 60;
static bool s_running;
static int64_t s_deadline_us;
static esp_timer_handle_t s_countdown;

static const int s_cube_minutes[] = {1, 5, 10, 15, 30, 60};

static void lock(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock(void)
{
    xSemaphoreGive(s_lock);
}

static void countdown_done_cb(void *arg)
{
    (void)arg;
    lock();
    const bool fire = s_running;
    if (fire) {
        s_running = false;
        s_remaining_sec = 0;
    }
    unlock();
    if (fire) {
        beep_alarm();
        ESP_LOGI(TAG, "timer done");
    }
}

/** Drop the running flag. Caller stops the hardware timer after releasing the lock. */
static bool timer_disarm_locked(void)
{
    if (!s_running) {
        return false;
    }
    const int64_t left = s_deadline_us - esp_timer_get_time();
    s_remaining_sec = (left > 0) ? (int)((left + 999999LL) / 1000000LL) : 0;
    s_running = false;
    return true;
}

static void timer_stop_hw(bool stop)
{
    if (!stop) {
        return;
    }
    const esp_err_t err = esp_timer_stop(s_countdown);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "timer stop %s", esp_err_to_name(err));
    }
}

static void recompute_mood_locked(void)
{
    pet_mood_t mood = PET_MOOD_CALM;
    if (s_person) {
        mood = PET_MOOD_HELLO;
    } else if (s_env.valid && s_env.temperature_c < PET_COLD_C) {
        mood = PET_MOOD_COLD;
    } else if (s_env.valid && s_env.humidity_rh >= PET_HUMID_RH) {
        mood = PET_MOOD_HUMID;
    }
    if (mood != s_mood) {
        ESP_LOGI(TAG, "mood %d -> %d", (int)s_mood, (int)mood);
        s_mood = mood;
    }
}

void app_state_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    configASSERT(s_lock);

    const esp_timer_create_args_t args = {
        .callback = countdown_done_cb,
        .name = "cube_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_countdown));
}

app_screen_t app_state_get_screen(void)
{
    lock();
    app_screen_t screen = s_screen;
    unlock();
    return screen;
}

pet_mood_t app_state_get_mood(void)
{
    lock();
    pet_mood_t mood = s_mood;
    unlock();
    return mood;
}

bool app_state_person_present(void)
{
    lock();
    bool present = s_person;
    unlock();
    return present;
}

void app_state_toggle_clock(void)
{
    lock();
    if (s_screen == APP_SCREEN_TIMER) {
        unlock();
        return;
    }
    s_screen = (s_screen == APP_SCREEN_PET) ? APP_SCREEN_CLOCK : APP_SCREEN_PET;
    s_screen_dirty = true;
    unlock();
    ESP_LOGI(TAG, "button A -> screen %d", (int)app_state_get_screen());
}

void app_state_toggle_timer(void)
{
    lock();
    const bool stop = timer_disarm_locked();
    if (s_screen == APP_SCREEN_TIMER) {
        s_screen = APP_SCREEN_PET;
    } else {
        s_screen = APP_SCREEN_TIMER;
        s_remaining_sec = s_cube_minutes[s_cube] * 60;
    }
    s_screen_dirty = true;
    const int screen = (int)s_screen;
    unlock();
    timer_stop_hw(stop);
    ESP_LOGI(TAG, "button B -> screen %d", screen);
}

void app_state_timer_start_stop(void)
{
    lock();
    if (s_screen != APP_SCREEN_TIMER) {
        unlock();
        return;
    }
    if (s_running) {
        const bool stop = timer_disarm_locked();
        const int left = s_remaining_sec;
        unlock();
        timer_stop_hw(stop);
        ESP_LOGI(TAG, "timer pause %d s", left);
        return;
    }
    if (s_remaining_sec <= 0) {
        s_remaining_sec = s_cube_minutes[s_cube] * 60;
    }
    const uint64_t us = (uint64_t)s_remaining_sec * 1000000ULL;
    s_deadline_us = esp_timer_get_time() + (int64_t)us;
    s_running = true;
    const int sec = s_remaining_sec;
    unlock();
    const esp_err_t err = esp_timer_start_once(s_countdown, us);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "timer start %s", esp_err_to_name(err));
        lock();
        s_running = false;
        unlock();
        return;
    }
    ESP_LOGI(TAG, "timer start %d s", sec);
}

bool app_state_pir_blocked(void)
{
    lock();
    const bool blocked = (s_screen == APP_SCREEN_CLOCK) || (s_screen == APP_SCREEN_TIMER);
    unlock();
    return blocked;
}

int app_state_timer_remaining_sec(void)
{
    lock();
    int sec = s_remaining_sec;
    if (s_running) {
        const int64_t left = s_deadline_us - esp_timer_get_time();
        sec = (left > 0) ? (int)((left + 999999LL) / 1000000LL) : 0;
    }
    unlock();
    return sec;
}

bool app_state_timer_running(void)
{
    lock();
    const bool running = s_running;
    unlock();
    return running;
}

cube_face_t app_state_cube_face(void)
{
    lock();
    const cube_face_t face = s_cube;
    unlock();
    return face;
}

int app_state_cube_minutes(cube_face_t face)
{
    if ((unsigned)face >= 6) {
        return s_cube_minutes[CUBE_FACE_5MIN];
    }
    return s_cube_minutes[face];
}

void app_state_set_cube_face(cube_face_t face)
{
    lock();
    if (face != s_cube) {
        s_cube = face;
        if (s_screen == APP_SCREEN_TIMER && !s_running) {
            s_remaining_sec = s_cube_minutes[face] * 60;
            ESP_LOGI(TAG, "cube face %d min", s_cube_minutes[face]);
        }
    }
    unlock();
}

void app_state_set_person(bool present)
{
    lock();
    if (present && ((s_screen == APP_SCREEN_CLOCK) || (s_screen == APP_SCREEN_TIMER))) {
        unlock();
        return;
    }
    if (present == s_person) {
        unlock();
        return;
    }
    s_person = present;
    recompute_mood_locked();
    unlock();

    if (present) {
        leds_on_person();
    }
    ESP_LOGI(TAG, "%s", present ? "person" : "alone");
}

void app_state_set_env(float temperature_c, float humidity_rh)
{
    lock();
    s_env.temperature_c = temperature_c;
    s_env.humidity_rh = humidity_rh;
    s_env.valid = true;
    s_env_dirty = true;
    recompute_mood_locked();
    unlock();
}

env_reading_t app_state_get_env(void)
{
    lock();
    env_reading_t env = s_env;
    unlock();
    return env;
}

void app_state_set_rotation(app_rotation_t rotation)
{
    lock();
    if (rotation != s_rot) {
        s_rot = rotation;
        s_rot_dirty = true;
        ESP_LOGI(TAG, "rotation %d", (int)rotation * 90);
    }
    unlock();
}

app_rotation_t app_state_get_rotation(void)
{
    lock();
    app_rotation_t rotation = s_rot;
    unlock();
    return rotation;
}

bool app_state_consume_screen_dirty(void)
{
    lock();
    bool dirty = s_screen_dirty;
    s_screen_dirty = false;
    unlock();
    return dirty;
}

bool app_state_consume_env_dirty(void)
{
    lock();
    bool dirty = s_env_dirty;
    s_env_dirty = false;
    unlock();
    return dirty;
}

bool app_state_consume_rot_dirty(void)
{
    lock();
    bool dirty = s_rot_dirty;
    s_rot_dirty = false;
    unlock();
    return dirty;
}
