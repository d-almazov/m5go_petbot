#include "ui.h"

#include <stdio.h>
#include <time.h>

#include "bsp/esp-bsp.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "app_state.h"

static const char *TAG = "ui";

typedef struct {
    lv_obj_t *clip;
    lv_obj_t *ball;
    lv_obj_t *pupil;
    lv_obj_t *glint;
} eye_t;

static lv_display_t *s_disp;
static lv_obj_t *s_screen;
static lv_obj_t *s_face_layer;
static lv_obj_t *s_visor;
static lv_obj_t *s_status;
static lv_obj_t *s_hint;
static eye_t s_eye_l;
static eye_t s_eye_r;
static lv_obj_t *s_smile;
static lv_obj_t *s_mouth_o;
static lv_obj_t *s_drop;
static lv_obj_t *s_clock_layer;
static lv_obj_t *s_time_label;
static lv_obj_t *s_temp_label;
static lv_obj_t *s_hum_label;
static lv_obj_t *s_timer_layer;
static lv_obj_t *s_cube_face;
static lv_obj_t *s_cube_time;
static lv_obj_t *s_cube_name;
static lv_obj_t *s_cube_state;
static lv_obj_t *s_cube_hint;

static app_screen_t s_shown = (app_screen_t)-1;
static pet_mood_t s_mood = PET_MOOD_CALM;
static int s_eye_open = 78;
static uint32_t s_eye_color = 0xF4FFF8;
static int s_pupil_d = 28;
static bool s_eyes_shut;

static void style_label(lv_obj_t *lbl, const lv_font_t *font, lv_color_t color)
{
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
}

static lv_obj_t *disc(lv_obj_t *parent, int d, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static void eye_style(eye_t *eye, int open_h, uint32_t color, int pupil_d)
{
    lv_obj_set_height(eye->clip, open_h);
    lv_obj_set_style_bg_color(eye->ball, lv_color_hex(color), 0);
    lv_obj_set_size(eye->pupil, pupil_d, pupil_d);
    if (open_h < 22) {
        lv_obj_add_flag(eye->pupil, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(eye->glint, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(eye->pupil, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(eye->glint, LV_OBJ_FLAG_HIDDEN);
    }
}

static void eyes_apply(int open_h)
{
    eye_style(&s_eye_l, open_h, s_eye_color, s_pupil_d);
    eye_style(&s_eye_r, open_h, s_eye_color, s_pupil_d);
}

static void pupils_look(int dx, int dy)
{
    lv_obj_align(s_eye_l.pupil, LV_ALIGN_CENTER, dx, dy);
    lv_obj_align(s_eye_r.pupil, LV_ALIGN_CENTER, dx, dy);
}

static void set_smile(int start, int end, uint32_t color, int width)
{
    lv_obj_remove_flag(s_smile, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_mouth_o, LV_OBJ_FLAG_HIDDEN);
    lv_arc_set_angles(s_smile, start, end);
    lv_obj_set_style_arc_width(s_smile, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_smile, lv_color_hex(color), LV_PART_INDICATOR);
}

static void apply_mood(pet_mood_t mood)
{
    s_mood = mood;
    s_eyes_shut = false;
    lv_obj_add_flag(s_drop, LV_OBJ_FLAG_HIDDEN);

    switch (mood) {
    case PET_MOOD_HELLO:
        s_eye_open = 78;
        s_eye_color = 0xD9FFE8;
        s_pupil_d = 22;
        set_smile(28, 152, 0x7CFFB0, 8);
        lv_label_set_text(s_status, "Hello!");
        style_label(s_status, &lv_font_montserrat_20, lv_color_hex(0x7CFFB0));
        lv_obj_remove_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        break;
    case PET_MOOD_COLD:
        s_eye_open = 52;
        s_eye_color = 0xD7F3FF;
        s_pupil_d = 24;
        lv_obj_add_flag(s_smile, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_color(s_mouth_o, lv_color_hex(0x9AD7FF), 0);
        lv_obj_remove_flag(s_mouth_o, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_status, "Brrr");
        style_label(s_status, &lv_font_montserrat_20, lv_color_hex(0x9AD7FF));
        lv_obj_remove_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        break;
    case PET_MOOD_HUMID:
        s_eye_open = 36;
        s_eye_color = 0xE7FFE8;
        s_pupil_d = 26;
        set_smile(210, 330, 0x8FD9A8, 6);
        lv_obj_remove_flag(s_drop, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_status, "Muggy");
        style_label(s_status, &lv_font_montserrat_20, lv_color_hex(0x8FD9A8));
        lv_obj_remove_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        break;
    case PET_MOOD_CALM:
    default:
        s_eye_open = 78;
        s_eye_color = 0xF4FFF8;
        s_pupil_d = 28;
        set_smile(46, 134, 0xB8FFD0, 5);
        lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        break;
    }
    eyes_apply(s_eye_open);
}

static void build_eye(eye_t *eye, lv_obj_t *parent, int x, int y)
{
    eye->clip = lv_obj_create(parent);
    lv_obj_set_size(eye->clip, 82, 78);
    lv_obj_set_pos(eye->clip, x, y);
    lv_obj_set_style_bg_opa(eye->clip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(eye->clip, 0, 0);
    lv_obj_set_style_pad_all(eye->clip, 0, 0);
    lv_obj_set_style_radius(eye->clip, 8, 0);
    lv_obj_remove_flag(eye->clip, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    eye->ball = disc(eye->clip, 74, 0xF4FFF8);
    lv_obj_align(eye->ball, LV_ALIGN_CENTER, 0, 0);
    eye->pupil = disc(eye->ball, 28, 0x101614);
    lv_obj_align(eye->pupil, LV_ALIGN_CENTER, 0, 2);
    eye->glint = disc(eye->ball, 10, 0xFFFFFF);
    lv_obj_align(eye->glint, LV_ALIGN_CENTER, -10, -12);
}

static void build_pet_face(lv_obj_t *parent)
{
    s_face_layer = lv_obj_create(parent);
    lv_obj_set_size(s_face_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_face_layer, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_face_layer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_face_layer, 0, 0);
    lv_obj_set_style_pad_all(s_face_layer, 0, 0);
    lv_obj_remove_flag(s_face_layer, LV_OBJ_FLAG_SCROLLABLE);

    /* 220px fits both landscape 320x240 and portrait 240x320. */
    s_visor = lv_obj_create(s_face_layer);
    lv_obj_set_size(s_visor, 220, 200);
    lv_obj_align(s_visor, LV_ALIGN_CENTER, 0, 4);
    lv_obj_set_style_bg_opa(s_visor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_visor, 0, 0);
    lv_obj_set_style_pad_all(s_visor, 0, 0);
    lv_obj_remove_flag(s_visor, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    build_eye(&s_eye_l, s_visor, 16, 8);
    build_eye(&s_eye_r, s_visor, 122, 8);

    s_smile = lv_arc_create(s_visor);
    lv_obj_set_size(s_smile, 86, 86);
    lv_obj_align(s_smile, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_arc_set_bg_angles(s_smile, 0, 0);
    lv_arc_set_angles(s_smile, 46, 134);
    lv_arc_set_rotation(s_smile, 0);
    lv_obj_set_style_arc_width(s_smile, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_smile, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_smile, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_smile, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_smile, lv_color_hex(0xB8FFD0), LV_PART_INDICATOR);
    lv_obj_set_style_opa(s_smile, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s_smile, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(s_smile, LV_OBJ_FLAG_CLICKABLE);

    s_mouth_o = lv_obj_create(s_visor);
    lv_obj_set_size(s_mouth_o, 18, 18);
    lv_obj_align(s_mouth_o, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_set_style_radius(s_mouth_o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_mouth_o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_mouth_o, 3, 0);
    lv_obj_set_style_border_color(s_mouth_o, lv_color_hex(0x9AD7FF), 0);
    lv_obj_remove_flag(s_mouth_o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_mouth_o, LV_OBJ_FLAG_HIDDEN);

    s_drop = disc(s_visor, 12, 0x8FD9A8);
    lv_obj_set_pos(s_drop, 168, 36);
    lv_obj_add_flag(s_drop, LV_OBJ_FLAG_HIDDEN);

    s_status = lv_label_create(s_face_layer);
    lv_label_set_text(s_status, "");
    lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);

    s_hint = lv_label_create(s_face_layer);
    lv_label_set_text(s_hint, "A clock    B timer");
    style_label(s_hint, &lv_font_montserrat_14, lv_color_hex(0x5A5A5A));
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, 0);

    apply_mood(PET_MOOD_CALM);
}

static void build_clock(lv_obj_t *parent)
{
    s_clock_layer = lv_obj_create(parent);
    lv_obj_set_size(s_clock_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_clock_layer, lv_color_hex(0x1B2A4A), 0);
    lv_obj_set_style_bg_opa(s_clock_layer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_clock_layer, 0, 0);
    lv_obj_set_style_pad_all(s_clock_layer, 0, 0);
    lv_obj_remove_flag(s_clock_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_clock_layer, LV_OBJ_FLAG_HIDDEN);

    s_time_label = lv_label_create(s_clock_layer);
    lv_label_set_text(s_time_label, "00:00:00");
    style_label(s_time_label, &lv_font_montserrat_48, lv_color_hex(0xF2F6FF));
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, -36);

    s_temp_label = lv_label_create(s_clock_layer);
    lv_label_set_text(s_temp_label, "Temp  --.- C");
    style_label(s_temp_label, &lv_font_montserrat_20, lv_color_hex(0x9AD0FF));
    lv_obj_align(s_temp_label, LV_ALIGN_CENTER, 0, 30);

    s_hum_label = lv_label_create(s_clock_layer);
    lv_label_set_text(s_hum_label, "Hum   --.- %");
    style_label(s_hum_label, &lv_font_montserrat_20, lv_color_hex(0x9AD0FF));
    lv_obj_align(s_hum_label, LV_ALIGN_CENTER, 0, 60);

    lv_obj_t *hint = lv_label_create(s_clock_layer);
    lv_label_set_text(hint, "Btn A: pet face");
    style_label(hint, &lv_font_montserrat_14, lv_color_hex(0x7A8AA8));
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static uint32_t cube_color(cube_face_t face)
{
    static const uint32_t colors[] = {
        0xE7ECF2, /* 1 min, screen up */
        0x7DFFB3, /* 5 min, buttons down */
        0x7EC8FF, /* 10 min */
        0xFFC56A, /* 15 min, buttons up */
        0xFF8B6A, /* 30 min */
        0xC9A6FF, /* 60 min, screen down */
    };
    if ((unsigned)face >= 6) {
        return colors[CUBE_FACE_5MIN];
    }
    return colors[face];
}

static void style_panel(lv_obj_t *obj, uint32_t color, int radius)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void build_timer(lv_obj_t *parent)
{
    s_timer_layer = lv_obj_create(parent);
    lv_obj_set_size(s_timer_layer, LV_PCT(100), LV_PCT(100));
    style_panel(s_timer_layer, 0x000000, 0);
    lv_obj_add_flag(s_timer_layer, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *shadow = lv_obj_create(s_timer_layer);
    lv_obj_set_size(shadow, 156, 156);
    style_panel(shadow, 0x1C1C1C, 22);
    lv_obj_align(shadow, LV_ALIGN_CENTER, 14, -26);

    s_cube_face = lv_obj_create(s_timer_layer);
    lv_obj_set_size(s_cube_face, 156, 156);
    style_panel(s_cube_face, cube_color(CUBE_FACE_5MIN), 22);
    lv_obj_align(s_cube_face, LV_ALIGN_CENTER, -8, -8);

    s_cube_time = lv_label_create(s_cube_face);
    lv_label_set_text(s_cube_time, "05:00");
    style_label(s_cube_time, &lv_font_montserrat_48, lv_color_hex(0x141414));
    lv_obj_align(s_cube_time, LV_ALIGN_CENTER, 0, -10);

    s_cube_name = lv_label_create(s_cube_face);
    lv_label_set_text(s_cube_name, "5 min");
    style_label(s_cube_name, &lv_font_montserrat_20, lv_color_hex(0x141414));
    lv_obj_align(s_cube_name, LV_ALIGN_CENTER, 0, 38);

    s_cube_state = lv_label_create(s_timer_layer);
    lv_label_set_text(s_cube_state, "Turn to set");
    style_label(s_cube_state, &lv_font_montserrat_14, lv_color_hex(0xD0D0D0));
    lv_obj_align(s_cube_state, LV_ALIGN_BOTTOM_MID, 0, -28);

    s_cube_hint = lv_label_create(s_timer_layer);
    lv_label_set_text(s_cube_hint, "B face    C start");
    style_label(s_cube_hint, &lv_font_montserrat_14, lv_color_hex(0x6A6A6A));
    lv_obj_align(s_cube_hint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void refresh_timer(void)
{
    static int shown_sec = -1;
    static int shown_face = -1;
    static int shown_run = -1;

    const int sec = app_state_timer_remaining_sec();
    const bool running = app_state_timer_running();
    const cube_face_t face = app_state_cube_face();
    const int full = app_state_cube_minutes(face) * 60;
    const int run_key = running ? 1 : (sec == 0 ? 2 : (sec == full ? 0 : 3));
    if (sec == shown_sec && (int)face == shown_face && run_key == shown_run) {
        return;
    }
    shown_sec = sec;
    shown_face = (int)face;
    shown_run = run_key;

    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", sec / 60, sec % 60);
    lv_label_set_text(s_cube_time, buf);
    snprintf(buf, sizeof(buf), "%d min", full / 60);
    lv_label_set_text(s_cube_name, buf);
    lv_obj_set_style_bg_color(s_cube_face, lv_color_hex(cube_color(face)), 0);

    const char *state = "Turn to set";
    const char *hint = "B face    C start";
    if (running) {
        state = "Running";
        hint = "B face    C stop";
    } else if (sec <= 0) {
        state = "Done";
    } else if (sec != full) {
        state = "Paused";
    }
    lv_label_set_text(s_cube_state, state);
    lv_label_set_text(s_cube_hint, hint);
}

static void show_screen(app_screen_t screen)
{
    lv_obj_add_flag(s_face_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_clock_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_timer_layer, LV_OBJ_FLAG_HIDDEN);
    if (screen == APP_SCREEN_CLOCK) {
        lv_obj_remove_flag(s_clock_layer, LV_OBJ_FLAG_HIDDEN);
    } else if (screen == APP_SCREEN_TIMER) {
        lv_obj_remove_flag(s_timer_layer, LV_OBJ_FLAG_HIDDEN);
        refresh_timer();
    } else {
        lv_obj_remove_flag(s_face_layer, LV_OBJ_FLAG_HIDDEN);
        apply_mood(app_state_get_mood());
    }
    s_shown = screen;
}

static void refresh_clock_labels(void)
{
    time_t now = 0;
    time(&now);
    struct tm t;
    localtime_r(&now, &t);

    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", &t);
    lv_label_set_text(s_time_label, buf);

    env_reading_t env = app_state_get_env();
    if (env.valid) {
        snprintf(buf, sizeof(buf), "Temp  %.1f C", env.temperature_c);
        lv_label_set_text(s_temp_label, buf);
        snprintf(buf, sizeof(buf), "Hum   %.1f %%", env.humidity_rh);
        lv_label_set_text(s_hum_label, buf);
    }
}

static void animate_face(void)
{
    static int tick;
    tick++;

    const bool blink = (tick % 64) < 3;
    if (blink != s_eyes_shut) {
        s_eyes_shut = blink;
        eyes_apply(blink ? 8 : s_eye_open);
    }

    if (!blink) {
        static const int look_x[] = {0, 0, 4, 4, 0, -5, -5, 0};
        static const int look_y[] = {2, 2, 1, 0, 2, 2, 4, 2};
        const int step = (tick / 18) % 8;
        pupils_look(look_x[step], look_y[step]);
    }

    int x = 0;
    int y = 4;
    switch (s_mood) {
    case PET_MOOD_HELLO: {
        static const int bob[] = {0, -2, -5, -7, -5, -2, 0, 2, 4, 2};
        y = 4 + bob[(tick / 2) % 10];
        break;
    }
    case PET_MOOD_COLD: {
        static const int shiver[] = {-4, -1, 3, 5, 1, -3, 4, -2};
        x = shiver[tick % 8];
        break;
    }
    case PET_MOOD_HUMID:
        lv_obj_set_y(s_drop, 30 + (tick % 26) * 2);
        break;
    case PET_MOOD_CALM:
    default:
        y = 4 + ((tick / 12) % 2 == 0 ? -1 : 1);
        break;
    }
    lv_obj_align(s_visor, LV_ALIGN_CENTER, x, y);
}

/* 270° clockwise from the accelerometer pose (90° plus another 180°). */
static lv_display_rotation_t to_lv_rot(app_rotation_t rotation)
{
    switch (rotation) {
    case APP_ROT_0:
        return LV_DISPLAY_ROTATION_270;
    case APP_ROT_90:
        return LV_DISPLAY_ROTATION_0;
    case APP_ROT_180:
        return LV_DISPLAY_ROTATION_90;
    case APP_ROT_270:
    default:
        return LV_DISPLAY_ROTATION_180;
    }
}

static void ui_tick(void *arg)
{
    (void)arg;
    if (!bsp_display_lock(40)) {
        return;
    }

    if (app_state_consume_rot_dirty()) {
        bsp_display_rotate(s_disp, to_lv_rot(app_state_get_rotation()));
    }

    if (app_state_consume_screen_dirty() || s_shown != app_state_get_screen()) {
        show_screen(app_state_get_screen());
    }

    if (s_shown == APP_SCREEN_CLOCK) {
        refresh_clock_labels();
        (void)app_state_consume_env_dirty();
    } else if (s_shown == APP_SCREEN_TIMER) {
        refresh_timer();
    } else {
        const pet_mood_t mood = app_state_get_mood();
        if (mood != s_mood) {
            apply_mood(mood);
        }
        animate_face();
        if (app_state_consume_env_dirty()) {
            /* Mood already follows the reading. */
        }
    }

    bsp_display_unlock();
}

static lv_display_t *display_start(void)
{
    const bsp_display_config_t panel_cfg = {
        .max_transfer_sz = (int)(BSP_LCD_DRAW_BUFF_SIZE * sizeof(uint16_t)),
    };
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;

    /*
     * bsp_display_new() enables the LCD feature, which switches the
     * backlight on. Its LEDC PWM channel must exist first.
     */
    if (bsp_display_brightness_init() != ESP_OK) {
        return NULL;
    }
    if (bsp_display_new(&panel_cfg, &panel, &io) != ESP_OK) {
        return NULL;
    }
    esp_lcd_panel_disp_on_off(panel, true);

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_affinity = 1;
    if (lvgl_port_init(&port_cfg) != ESP_OK) {
        return NULL;
    }

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = true,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .swap_bytes = (BSP_LCD_BIGENDIAN ? 1 : 0),
        },
    };

    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (disp != NULL) {
        bsp_display_backlight_on();
    }
    return disp;
}

esp_err_t ui_start(void)
{
    s_disp = display_start();
    if (s_disp == NULL) {
        ESP_LOGE(TAG, "display start failed");
        return ESP_FAIL;
    }

    /*
     * LVGL is already running in its own FreeRTOS task.  Rotation causes a
     * full-screen invalidation, so it must be protected by the same port
     * mutex as every other LVGL call.
     */
    bsp_display_lock(0);
    bsp_display_rotate(s_disp, to_lv_rot(app_state_get_rotation()));
    (void)app_state_consume_rot_dirty();
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    build_pet_face(s_screen);
    build_clock(s_screen);
    build_timer(s_screen);
    show_screen(APP_SCREEN_PET);
    lv_screen_load(s_screen);
    bsp_display_unlock();

    const esp_timer_create_args_t tick_args = {
        .callback = ui_tick,
        .name = "ui_tick",
    };
    esp_timer_handle_t tick;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick, 50 * 1000));

    ESP_LOGI(TAG, "UI ready");
    return ESP_OK;
}
