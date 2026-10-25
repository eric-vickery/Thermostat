#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_IDLE = 2,
    _SCREEN_ID_LAST = 2
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *idle;
    lv_obj_t *main_container;
    lv_obj_t *current_temp_arc;
    lv_obj_t *setpoint_arc;
    lv_obj_t *current_temp_label;
    lv_obj_t *current_temp;
    lv_obj_t *setpoint_label;
    lv_obj_t *setpoint;
    lv_obj_t *mode_button;
    lv_obj_t *mode_icon_off;
    lv_obj_t *mode_icon_manual;
    lv_obj_t *mode_icon_schedule;
    lv_obj_t *humidity_label;
    lv_obj_t *humidity;
    lv_obj_t *pressure_label;
    lv_obj_t *pressure;
    lv_obj_t *current_temp_arc_1;
    lv_obj_t *current_temp_1;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_idle();
void tick_screen_idle();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/