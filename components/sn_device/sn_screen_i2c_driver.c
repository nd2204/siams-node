#include "display/lv_display.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "sn_common.h"
#include "sn_driver/port_desc.h"
#include "sn_json.h"
#include "sn_driver_registry.h"

#include "lvgl.h"
#include "driver/i2c_master.h"
#include "freertos/idf_additions.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdbool.h>
#include <string.h>
#include <sys/lock.h>
#include <unistd.h>

struct screen_i2c_ctx_s {
  uint8_t *fb;
  uint8_t *internal_fb;
  lv_display_t *display;
  esp_lcd_panel_handle_t panel_handle;
  TaskHandle_t lvgl_port_task;
  _lock_t lvgl_api_lock;
};

static const char *TAG = "SCREEN_I2C_DRIVER";
static const char *screen_i2c_types[] = {"ssd1306", ((void *)0)};

static inline void lvgl_lock(screen_i2c_ctx_t *ctx) { _lock_acquire(&ctx->lvgl_api_lock); }
static inline void lvgl_unlock(screen_i2c_ctx_t *ctx) { _lock_release(&ctx->lvgl_api_lock); }

static const sn_param_desc_t params_desc[] = {
  {.name = "msg", .enum_values = NULL, .type = PTYPE_STRING, .required = true},
  {.name = NULL}  // NULL terminate
};
static const sn_command_desc_t schema = {
  .action = "set_message",
  .params = params_desc,
};

static bool notify_lvgl_flush_ready(
  esp_lcd_panel_io_handle_t io_panel, esp_lcd_panel_io_event_data_t *edata, void *user_ctx
) {
  lv_display_t *disp = (lv_display_t *)user_ctx;
  lv_display_flush_ready(disp);
  return false;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  screen_i2c_ctx_t *ctx = lv_display_get_user_data(disp);

  // This is necessary because LVGL reserves 2 x 4 bytes in the buffer, as these
  // are assumed to be used as a palette. Skip the palette here More information
  // about the monochrome, please refer to
  // https://docs.lvgl.io/9.2/porting/display.html#monochrome-displays
  px_map += 8;

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
      uint8_t *buf = ctx->fb + hor_res * (y >> 3) + (x);
      if (chroma_color) {
        (*buf) &= ~(1 << (y % 8));
      } else {
        (*buf) |= (1 << (y % 8));
      }
    }
  }

  // pass the draw buffer to the driver
  esp_lcd_panel_draw_bitmap(ctx->panel_handle, x1, y1, x2 + 1, y2 + 1, ctx->fb);
}

static void increase_lvgl_tick(void *arg) {
  /* Tell LVGL how many milliseconds has elapsed */
  lv_tick_inc(5);
}

static void lvgl_port_task(void *pvParams) {
  ESP_LOGI(TAG, "Starting LVGL task");
  screen_i2c_ctx_t *ctx = (screen_i2c_ctx_t *)pvParams;
  uint32_t time_till_next_ms = 0;
  for (;;) {
    lvgl_lock(ctx);
    time_till_next_ms = lv_timer_handler();
    lvgl_unlock(ctx);
    // in case of triggering a task watch dog time out
    time_till_next_ms = MAX(time_till_next_ms, 500);
    // in case of lvgl display not ready yet
    time_till_next_ms = MIN(time_till_next_ms, 1000 / CONFIG_FREERTOS_HZ);
    usleep(1000 * time_till_next_ms);
  }
}

static bool screen_i2c_probe(const sn_device_port_desc_t *port) {
  // TODO: implement probing
  return true;
}

static esp_err_t screen_i2c_init(
  const sn_device_port_desc_t *desc, void *ctx_out, size_t ctx_size
) {
  if (!desc || !ctx_out || ctx_size < (sizeof(screen_i2c_ctx_t))) {
    return ESP_ERR_INVALID_ARG;
  }

  const sn_actuator_port_t *port = &desc->desc.a;
  sn_port_usage_type_e ut = port->usage_type;
  if (ut != PUT_I2C) {
    ESP_LOGE(TAG, "Invalid port usage %d, expecting %d (%s)", ut, PUT_GPIO, TO_STR(PUT_I2C));
    return ESP_ERR_INVALID_ARG;
  }

  // WARN: This should be placed in the registration code
  // I only placed it here for quick demo
  ESP_LOGI(TAG, "Initialize I2C bus");
  i2c_master_bus_handle_t i2c_bus = NULL;
  i2c_master_bus_config_t bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .i2c_port = I2C_NUM_0,
    .sda_io_num = port->usage.i2c.sda,
    .scl_io_num = port->usage.i2c.scl,
    .flags.enable_internal_pullup = true,
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

  screen_i2c_ctx_t *ctx = ctx_out;
  memset(ctx, 0, sizeof(screen_i2c_ctx_t));
  if (strncmp("ssd1306", desc->drv_name, 7) == 0) {
    const int ssd1306_h_res = 128;
    const int ssd1306_v_res = 64;
    const int buffer_size = ssd1306_v_res * ssd1306_h_res / 8;

    ctx->fb = malloc(buffer_size);

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
      .dev_addr = 0x3C, // default for ssd1306
      .scl_speed_hz = 400 * 1000,
      .control_phase_bytes = 1, // According to SSD1306 datasheet
      .lcd_cmd_bits = 8,        // According to SSD1306 datasheet
      .lcd_param_bits = 8,      // According to SSD1306 datasheet
      .dc_bit_offset = 6,       // According to SSD1306 datasheet
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    // ----------------------------------------
    // Install panel driver
    // ----------------------------------------
    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
      .bits_per_pixel = 1,
      .reset_gpio_num = -1,
    };
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
      .height = 64,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &ctx->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(ctx->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(ctx->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(ctx->panel_handle, true));

    // ----------------------------------------
    // Init LVGL
    // ----------------------------------------
    ESP_LOGI(TAG, "Initialize LVGL");
    lv_init();
    // create a lvgl display

    ctx->display = lv_display_create(ssd1306_h_res, ssd1306_v_res);
    // associate the i2c panel handle to the display
    lv_display_set_user_data(ctx->display, ctx);
    /* allocate LVGL draw buffer and setup display */
    size_t draw_buffer_sz = buffer_size + 8;
    ctx->internal_fb = heap_caps_calloc(1, draw_buffer_sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(ctx->internal_fb);

    // LVGL9 suooprt new monochromatic format.
    lv_display_set_color_format(ctx->display, LV_COLOR_FORMAT_I1);
    // initialize LVGL draw buffers
    lv_display_set_buffers(
      ctx->display, ctx->internal_fb, NULL, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_FULL
    );
    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(ctx->display, lvgl_flush_cb);

    ESP_LOGI(TAG, "Register io panel event callback for LVGL flush ready notification");
    const esp_lcd_panel_io_callbacks_t cbs = {
      .on_color_trans_done = notify_lvgl_flush_ready,
    };

    /* Register done callback */
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, ctx->display);

    ESP_LOGI(TAG, "Use esp_timer as LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args = {
      .callback = &increase_lvgl_tick,
      .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 5 * 1000));

    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(lvgl_port_task, "LVGL", 4096, ctx, 5, &ctx->lvgl_port_task);

    lvgl_lock(ctx);
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "hello world");
    lv_obj_set_pos(label, 0, 0);
    lvgl_unlock(ctx);

    ESP_LOGI(TAG, "screen i2c init port=%s", desc->port_name);
  } else {
    return ESP_ERR_NOT_SUPPORTED;
  }

  return ESP_OK;
}

static void screen_i2c_deinit(void *ctxv) {
  // screen_i2c_ctx_t *ctx = (screen_i2c_ctx_t *)ctxv;
  // heap_caps_free(ctx->fb);
}

static esp_err_t screen_i2c_controller(void *ctxv, const cJSON *paramsJson, cJSON **out_result) {
  if (!ctxv || !paramsJson) return ESP_ERR_INVALID_ARG;
  screen_i2c_ctx_t *ctx = ctxv;

  if (!validate_params_json(params_desc, paramsJson, out_result)) {
    return ESP_ERR_INVALID_ARG;
  }

  const char *msg = NULL;
  if (json_get_string(paramsJson, "msg", &msg)) {
    lvgl_lock(ctx);
    lv_obj_clean(lv_screen_active());
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, msg);
    lv_obj_set_pos(label, 0, 0);
    lvgl_unlock(ctx);
  }

  return ESP_OK;
}

const sn_driver_desc_t screen_i2c_driver = {
  .name = "screen_i2c_drv",
  .supported_types = screen_i2c_types,
  .priority = 50,
  .probe = screen_i2c_probe,
  .init = screen_i2c_init,
  .deinit = screen_i2c_deinit,
  .read_multi = NULL,
  .control = screen_i2c_controller,
  .command_desc = &schema
};
