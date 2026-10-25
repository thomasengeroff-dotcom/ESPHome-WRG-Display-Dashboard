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
    _SCREEN_ID_LAST = 1
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *l_ftung;
    lv_obj_t *dg;
} objects_t;

extern objects_t objects;

typedef struct {
    lv_meter_scale_t *Temperatur;
    lv_meter_indicator_t *indicator;
} screen_main_state_t;

extern screen_main_state_t screen_main_state;

void create_screen_main(lv_obj_t *parent);
void tick_screen_main();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/