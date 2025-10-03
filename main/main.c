#include "misc/lv_async.h"
#include "sn_config.h"
#include "sn_devmgr.h"
#include "sn_sysmgr.h"
#include "sn_ui.h"
#include "sn_wifi.h"

#include "sdkconfig.h"
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/param.h>

#include "sn_sensors.h"

static const char *TAG = "main";

#define I2C_PORT I2C_NUM_0

void demo_ui(void *params) {
  ESP_LOGI(TAG, "Display LVGL Scroll Text");
  lv_display_t *disp = sn_ui_get_display();
  lv_obj_t *scr = lv_display_get_screen_active(disp);
  lv_obj_t *label = lv_label_create(scr);
  lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll */
  lv_label_set_text(label, "Hello cruel world");
  /* Size of the screen (if you use rotation 90 or 270, please use
   * lv_display_get_vertical_resolution) */
  lv_obj_set_width(label, lv_display_get_horizontal_resolution(disp));
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void app_main(void) {
  sn_sysmgr_init_self();
  SN_SYSMGR_REGISTER("sn-devmgr", sn_devmgr_init, NULL);
  SN_SYSMGR_REGISTER("sn-sensors", sn_sensors_init, NULL);
  SN_SYSMGR_REGISTER("sn-ui", sn_ui_init, NULL);
  // SN_SYSMGR_REGISTER("sn-wifi", sn_wifi_init, NULL);
  sn_sysmgr_init_all();

  if (sn_sysmgr_is_valid()) {
    // sn_wifi_connect(CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);

    xTaskCreatePinnedToCore(
      ui_task, "ui-task", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL, 0
    );
    // xTaskCreatePinnedToCore(read_bh1750_task, "light-task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(read_dht_task, "dht-task", 4096, NULL, 5, NULL, 1);

    lv_async_call(demo_ui, NULL);
  }

  sn_sysmgr_shutdown_all();
}
