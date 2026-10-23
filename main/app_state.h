#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Below this temperature the pet looks cold. */
#define PET_COLD_C       20.0f
/** At or above this humidity the pet looks muggy. */
#define PET_HUMID_RH     70.0f

typedef enum {
    APP_SCREEN_PET = 0,
    APP_SCREEN_CLOCK,
    APP_SCREEN_TIMER,
} app_screen_t;

/** Six faces of the cube timer, chosen by how the M5GO is held. */
typedef enum {
    CUBE_FACE_1MIN = 0,  /**< Screen facing up */
    CUBE_FACE_5MIN,      /**< Buttons along the bottom */
    CUBE_FACE_10MIN,     /**< Turned on one side */
    CUBE_FACE_15MIN,     /**< Buttons along the top */
    CUBE_FACE_30MIN,     /**< Turned on the other side */
    CUBE_FACE_60MIN,     /**< Screen facing down */
} cube_face_t;

typedef enum {
    PET_MOOD_CALM = 0,   /**< Soft smile */
    PET_MOOD_HELLO,      /**< Person in view */
    PET_MOOD_COLD,       /**< Temperature under PET_COLD_C */
    PET_MOOD_HUMID,      /**< Humidity at or above PET_HUMID_RH */
} pet_mood_t;

/** Display rotation that keeps the face upright. 90 = buttons along the bottom. */
typedef enum {
    APP_ROT_0 = 0,
    APP_ROT_90,
    APP_ROT_180,
    APP_ROT_270,
} app_rotation_t;

typedef struct {
    float temperature_c;
    float humidity_rh;
    bool valid;
} env_reading_t;

void app_state_init(void);

app_screen_t app_state_get_screen(void);
pet_mood_t app_state_get_mood(void);
bool app_state_person_present(void);

/** Toggle Button A between the pet face and the clock. Ignored in timer mode. */
void app_state_toggle_clock(void);

/** Button B: enter the cube timer, or leave it for the pet face. */
void app_state_toggle_timer(void);

/** Button C: start or pause the cube timer. Ignored outside timer mode. */
void app_state_timer_start_stop(void);

/** True on the clock and the cube timer, where PIR must do nothing. */
bool app_state_pir_blocked(void);

int app_state_timer_remaining_sec(void);
bool app_state_timer_running(void);
cube_face_t app_state_cube_face(void);
int app_state_cube_minutes(cube_face_t face);

/**
 * Live PIR presence.
 * Rising edge shows the hello face.
 * Ignored while the clock or the cube timer is up.
 */
void app_state_set_person(bool present);

/** Update the cube face from the accelerometer. Changes the time only while paused. */
void app_state_set_cube_face(cube_face_t face);

void app_state_set_env(float temperature_c, float humidity_rh);
env_reading_t app_state_get_env(void);

void app_state_set_rotation(app_rotation_t rotation);
app_rotation_t app_state_get_rotation(void);

bool app_state_consume_screen_dirty(void);
bool app_state_consume_env_dirty(void);
bool app_state_consume_rot_dirty(void);

#ifdef __cplusplus
}
#endif
