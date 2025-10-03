#ifndef SN_UI_H
#define SN_UI_H

#include "esp_err.h"
#include "lvgl.h"

esp_err_t sn_ui_init();

void ui_task();

lv_display_t *sn_ui_get_display();

#endif
