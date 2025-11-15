#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "sn_common.h"
#include "sn_driver/port_desc.h"
#include "sn_json.h"
#include "sn_driver_registry.h"

#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include <stdbool.h>
#include <string.h>

#define LEDC_TIMER    LEDC_TIMER_0
#define LEDC_MODE     LEDC_LOW_SPEED_MODE
#define LEDC_RES      LEDC_TIMER_13_BIT
#define LEDC_FREQ     5000
#define LEDC_MAX_DUTY ((1 << 13) - 1)

static const char *TAG = "SN_LED_DRIVER";
static const char *led_types[] = {"led", NULL};
static uint8_t led_count = 0;

typedef enum { LED_MODE_STATIC, LED_MODE_BREATHING } led_mode_t;

struct led_ctx_s {
  TaskHandle_t led_task;
  gpio_num_t pin;
  float brightness; // 0.0–1.0
  led_mode_t mode;
  uint8_t channel;
  bool on;
};

static void led_task(void *pvParams) {
  led_ctx_t *ctx = (led_ctx_t *)pvParams;

  for (;;) {
    if (ctx) {
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ----------------------------------------------------

static const sn_param_desc_t params_desc[] = {
  {
   .name = "enable",
   .type = PTYPE_BOOL,
   .required = true,
   },
  {
   .name = "brightness",
   .type = PTYPE_NUMBER,
   .min = 0,
   .max = 1,
   .required = false,
   },
  {.name = NULL}  // NULL terminate
};

static const sn_command_desc_t schema = {
  .action = "control_led",
  .params = params_desc,
};

// ----------------------------------------------------
static bool led_probe(const sn_device_port_desc_t *port) {
  if (!port) return false;
  // light probe: if pin >=0 assume possible
  return (port->desc.a.usage.gpio.pin >= 0);
}

// ----------------------------------------------------
static inline void led_set_brightness(float value, ledc_channel_t channel) {
  value = CLAMP(value, 0.0f, 1.0f);
  channel = CLAMP(channel, LEDC_CHANNEL_0, LEDC_CHANNEL_MAX);
  if (channel > LEDC_CHANNEL_MAX) {
    ESP_LOGE(TAG, "Maximum led channel reached (%d/%d)", channel, LEDC_CHANNEL_MAX);
  };
  uint32_t duty = (uint32_t)(value * LEDC_MAX_DUTY);
  ledc_set_duty(LEDC_MODE, channel, duty);
  ledc_update_duty(LEDC_MODE, channel);
}

#define STR(token) #token

// --------------------------------------------------------------------------------
// Initialize
// --------------------------------------------------------------------------------

static esp_err_t led_init(const sn_device_port_desc_t *desc, void *ctx_out, size_t ctx_size) {
  if (!desc || !ctx_out || ctx_size < sizeof(led_ctx_t)) {
    return ESP_ERR_INVALID_ARG;
  }

  const sn_actuator_port_t *port = &desc->desc.a;

  sn_port_usage_type_e ut = port->usage_type;
  if (ut != PUT_GPIO) {
    ESP_LOGE(TAG, "Invalid port usage %d, expecting %d (%s)", ut, PUT_GPIO, STR(PUT_GPIO));
    return ESP_ERR_INVALID_ARG;
  }

  gpio_num_t _pin = port->usage.gpio.pin;

  ledc_timer_config_t timer_conf = {
    .speed_mode = LEDC_MODE,
    .timer_num = LEDC_TIMER,
    .duty_resolution = LEDC_RES,
    .freq_hz = LEDC_FREQ,
    .clk_cfg = LEDC_AUTO_CLK
  };

  // --- Configure LEDC timer ---
  ledc_timer_config(&timer_conf);

  // --- Configure LEDC channel ---
  ledc_channel_config_t ch_conf = {
    .gpio_num = _pin,
    .speed_mode = LEDC_MODE,
    .channel = LEDC_CHANNEL_0 + led_count,
    .timer_sel = LEDC_TIMER,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&ch_conf);

  // clang-format off
  led_ctx_t init_cfg = {
    .pin = _pin,
    .led_task = NULL,
    .on = false,
    .brightness = 0,
    .channel = LEDC_CHANNEL_0 + led_count
  };

  led_count++;

  // clang-format on
  memcpy(ctx_out, &init_cfg, sizeof(led_ctx_t));

  ESP_LOGI(TAG, "Led init port=%s pin=%d", desc->port_name, _pin);
  return ESP_OK;
}

// --------------------------------------------------------------------------------
// Clean up
// --------------------------------------------------------------------------------
static void led_deinit(void *ctx) { (void)ctx; }

// --------------------------------------------------------------------------------
// Controller handler
// --------------------------------------------------------------------------------
static esp_err_t led_controller(void *ctxv, const cJSON *paramsJson, cJSON **out_result) {
  if (!paramsJson) return ESP_ERR_INVALID_ARG;
  if (!ctxv) {
    ESP_LOGE(TAG, "Context is null");
    return ESP_ERR_INVALID_STATE;
  }
  led_ctx_t *ctx = (led_ctx_t *)ctxv;
  if (!paramsJson) return ESP_ERR_INVALID_ARG;
  if (!validate_params_json(params_desc, paramsJson, out_result)) {
    return ESP_ERR_INVALID_ARG;
  }

  // apply conditionally
  bool on = ctx->on;
  if (json_get_bool(paramsJson, "enable", &on)) {
    ctx->on = on;
  }
  double value = 0;
  if (json_get_number(paramsJson, "brightness", &value)) {
    ctx->brightness = CLAMP(value, 0.0f, 1.0f);
  }

  // Set light brightness
  if (ctx->on) {
    led_set_brightness(ctx->brightness, ctx->channel);
  } else {
    led_set_brightness(0, ctx->channel);
  }

  if (out_result)
    *out_result =
      build_success_fmt("enable=%s, brightness=%.2f", ctx->on ? "true" : "false", ctx->brightness);
  return ESP_OK;
}

// --------------------------------------------------------------------------------
// Export driver desc
// --------------------------------------------------------------------------------
const sn_driver_desc_t led_driver = {
  .name = "led_drv",
  .supported_types = led_types,
  .priority = 50,
  .probe = led_probe,
  .init = led_init,
  .deinit = led_deinit,
  .read_multi = NULL,
  .control = led_controller,
  .command_desc = &schema
};
