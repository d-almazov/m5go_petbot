#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t beep_init(void);

/** Short friendly chirp. Non-blocking (spawns a task). */
void beep_hello(void);

/** Three-note chime when the cube timer reaches zero. Non-blocking. */
void beep_alarm(void);

#ifdef __cplusplus
}
#endif
