#include "sn_ui.h"
#include "misc/lv_async.h"
#include "sn_config.h"
#include "sn_devmgr.h"
#include "sn_error.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

#include <stdio.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

static const char *TAG = "sn-ui";

// LVGL reserves 2 x 4 bytes in the buffer, as these are assumed to be used as a
// palette.
static const size_t kFrameBufferSize = LCD_H_RES * LCD_V_RES / 8 + LVGL_PALETTE_SIZE;
// To use LV_COLOR_FORMAT_I1, we need an extra buffer to hold the converted data
static uint8_t oled_buffer[LCD_H_RES * LCD_V_RES / 8];
static void *oled_back_buffer = NULL;
static lv_display_t *g_display = NULL;

// LVGL library is not thread-safe, this example will call LVGL APIs from
// different tasks, so use a mutex to protect it
// static SemaphoreHandle_t lvgl_api_lock = NULL;
static esp_lcd_panel_handle_t g_panelHandle = NULL;
static esp_lcd_panel_io_handle_t g_ioHandle = NULL;

static void increase_lvgl_tick(void *arg) {
  /* Tell LVGL how many milliseconds has elapsed */
  lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static bool notify_lvgl_flush_ready(
  esp_lcd_panel_io_handle_t io_panel, esp_lcd_panel_io_event_data_t *edata, void *user_ctx
) {
  lv_display_t *disp = (lv_display_t *)user_ctx;
  lv_display_flush_ready(disp);
  return false;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);

  // This is necessary because LVGL reserves 2 x 4 bytes in the buffer, as these
  // are assumed to be used as a palette. Skip the palette here More information
  // about the monochrome, please refer to
  // https://docs.lvgl.io/9.2/porting/display.html#monochrome-displays
  px_map += LVGL_PALETTE_SIZE;

  uint16_t hor_res = lv_display_get_physical_horizontal_resolution(disp);
  int x1 = area->x1;
  int x2 = area->x2;
  int y1 = area->y1;
  int y2 = area->y2;

  for (int y = y1; y <= y2; y++) {
    for (int x = x1; x <= x2; x++) {
      /* The order of bits is MSB first
                  MSB           LSB
         bits      7 6 5 4 3 2 1 0
         pixels    0 1 2 3 4 5 6 7
                  Left         Right
      */
      bool chroma_color = (px_map[(hor_res >> 3) * y + (x >> 3)] & 1 << (7 - x % 8));

      /* Write to the buffer as required for the display.
       * It writes only 1-bit for monochrome displays mapped vertically.*/
      uint8_t *buf = oled_buffer + hor_res * (y >> 3) + (x);
      if (chroma_color) {
        (*buf) &= ~(1 << (y % 8));
      } else {
        (*buf) |= (1 << (y % 8));
      }
    }
  }
  // pass the draw buffer to the driver
  SemaphoreHandle_t i2c_mutex = sn_devmgr_get_i2c_master_bus_mutex();
  if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, oled_buffer);
    xSemaphoreGive(i2c_mutex);
  } else {
    // timeout: schedule a retry or drop the frame (log)
    ESP_LOGW(TAG, "i2c mutex timeout in lvgl_flush_cb");
  }
}

static esp_err_t init_ssd1306_panel() {
  ESP_LOGI(TAG, "Install panel IO");
  esp_lcd_panel_io_i2c_config_t io_config = {
    .dev_addr = I2C_HW_ADDR,
    .scl_speed_hz = LCD_PIXEL_CLOCK_HZ,
    .control_phase_bytes = 1,       // According to SSD1306 datasheet
    .lcd_cmd_bits = LCD_CMD_BITS,   // According to SSD1306 datasheet
    .lcd_param_bits = LCD_CMD_BITS, // According to SSD1306 datasheet
    .dc_bit_offset = 6,             // According to SSD1306 datasheet
  };

  TRY(esp_lcd_new_panel_io_i2c(sn_devmgr_get_i2c_master_bus_handle(), &io_config, &g_ioHandle));

  ESP_LOGI(TAG, "Install SSD1306 panel driver");
  esp_lcd_panel_dev_config_t panel_config = {
    .bits_per_pixel = 1,
    .reset_gpio_num = PIN_NUM_RST,
  };
  esp_lcd_panel_ssd1306_config_t ssd1306_config = {
    .height = LCD_V_RES,
  };
  panel_config.vendor_config = &ssd1306_config;

  TRY(esp_lcd_new_panel_ssd1306(g_ioHandle, &panel_config, &g_panelHandle));
  TRY(esp_lcd_panel_reset(g_panelHandle));
  TRY(esp_lcd_panel_init(g_panelHandle));
  return esp_lcd_panel_disp_on_off(g_panelHandle, true);
}

static esp_err_t init_lgvl() {
  ESP_LOGI(TAG, "Initialize LVGL");
  lv_init();
  // create a lvgl display
  g_display = lv_display_create(LCD_H_RES, LCD_V_RES);
  // associate the i2c panel handle to the display
  lv_display_set_user_data(g_display, g_panelHandle);
  // create draw buffer
  oled_back_buffer = heap_caps_calloc(1, kFrameBufferSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  RETURN_IF_FALSE(TAG, oled_back_buffer, ESP_FAIL, "Cannot allocate framebuffer for display");

  // LVGL9 suooprt new monochromatic format.
  lv_display_set_color_format(g_display, LV_COLOR_FORMAT_I1);
  // initialize LVGL draw buffers
  lv_display_set_buffers(
    g_display, oled_back_buffer, NULL, kFrameBufferSize, LV_DISPLAY_RENDER_MODE_FULL
  );
  // set the callback which can copy the rendered image to an area of the
  // display
  lv_display_set_flush_cb(g_display, lvgl_flush_cb);

  ESP_LOGI(TAG, "Register io panel event callback for LVGL flush ready notification");
  const esp_lcd_panel_io_callbacks_t cbs = {
    .on_color_trans_done = notify_lvgl_flush_ready,
  };
  /* Register done callback */
  return esp_lcd_panel_io_register_event_callbacks(g_ioHandle, &cbs, g_display);
}

esp_err_t sn_ui_init() {
  // lvgl_api_lock = xSemaphoreCreateMutex();
  // RETURN_IF_FALSE(TAG, !lvgl_api_lock, ESP_FAIL, "Cannot create semaphore");
  RETURN_IF_FALSE(
    TAG, sn_devmgr_i2c_scan_address(0x3c) == ESP_OK, ESP_FAIL,
    "cannot find slave address %.2x. quitting task", 0x3c
  );

  TRY(init_ssd1306_panel());

  return init_lgvl();
}

lv_display_t *sn_ui_get_display() { return g_display; }

void ui_task() {
  ESP_LOGI(TAG, "Use esp_timer as LVGL tick timer");
  const esp_timer_create_args_t lvgl_tick_timer_args = {
    .callback = &increase_lvgl_tick,
    .name = "lvgl_tick"
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

  ESP_LOGI(TAG, "Starting LVGL task");
  uint32_t time_till_next_ms = 0;

  while (1) {
    time_till_next_ms = lv_timer_handler();
    // in case of triggering a task watch dog time out
    time_till_next_ms = MAX(time_till_next_ms, LVGL_TASK_MIN_DELAY_MS);
    // in case of lvgl display not ready yet
    time_till_next_ms = MIN(time_till_next_ms, LVGL_TASK_MAX_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(time_till_next_ms));
  }
}
