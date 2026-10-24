#ifndef EEZ_LVGL_UI_GUI_H
#define EEZ_LVGL_UI_GUI_H

#include <lvgl.h>

#include <eez/flow/lvgl_api.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t assets[1636];

void ui_init();
void ui_tick();

#ifdef __cplusplus
}
#endif

#endif // EEZ_LVGL_UI_GUI_H