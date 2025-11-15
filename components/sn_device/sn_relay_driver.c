#include "sn_driver_registry.h"
#include "sn_json.h"
#include "driver/gpio.h"
#include <stdint.h>

static const char *TAG = "SN_RELAY_DRIVER";
static const char *relay_types[] = {"relay", NULL};

struct relay_ctx_s {
  gpio_num_t pin;
  TaskHandle_t task;
  QueueHandle_t queue;
  int on_level; // some relay required 0v to turn on
  int off_level;
  bool on;
};

typedef enum { RELAY_CMD_ON, RELAY_CMD_OFF, RELAY_CMD_TOGGLE, RELAY_CMD_PULSE } relay_cmd_t;

typedef struct {
  relay_cmd_t cmd;
  uint32_t duration_ms;
} relay_msg_t;

static void relay_task(void *pvParams) {
  if (!pvParams) vTaskDelete(NULL);
  relay_ctx_t *ctx = (relay_ctx_t *)pvParams;
  relay_msg_t msg;
  for (;;) {
    while (xQueueReceive(ctx->queue, &msg, portMAX_DELAY) == pdTRUE) {
      switch (msg.cmd) {
        case RELAY_CMD_ON:
          gpio_set_level(ctx->pin, ctx->on_level);
          break;
        case RELAY_CMD_OFF:
          gpio_set_level(ctx->pin, ctx->off_level);
          break;
        case RELAY_CMD_TOGGLE:
          gpio_set_level(ctx->pin, !gpio_get_level(ctx->pin));
          break;
        case RELAY_CMD_PULSE:
          gpio_set_level(ctx->pin, ctx->on_level);
          vTaskDelay(pdMS_TO_TICKS(msg.duration_ms));
          gpio_set_level(ctx->pin, ctx->off_level);
          break;
      }
    }
  }
}

int relay_send(QueueHandle_t relayQueue, relay_cmd_t cmd, uint32_t duration_ms) {
  relay_msg_t msg = {.cmd = cmd, .duration_ms = duration_ms};
  return xQueueSend(relayQueue, &msg, 0);
}

static const sn_param_desc_t params_desc[] = {
  {.name = "enable", .required = true, .type = PTYPE_BOOL},
  {.name = "duration_sec", .required = false, .type = PTYPE_NUMBER, .min = 0.0f, .max = 1600.0f},
  {.name = NULL}
};

static const sn_command_desc_t schema = {
  .action = "control_relay",
  .params = params_desc,
};

static _Bool relay_probe(const sn_device_port_desc_t *port) { return true; }

static esp_err_t relay_init(const sn_device_port_desc_t *port, void *ctx_out, size_t ctx_size) {
  if (!port
      || !ctx_out
      || ctx_size < sizeof(relay_ctx_t)
      || port->desc.a.usage_type != PUT_GPIO
      || port->drv_type != DRIVER_TYPE_ACTUATOR)
    return ESP_ERR_INVALID_ARG;

  gpio_num_t pin = port->desc.a.usage.gpio.pin;
  gpio_config_t cfg = {
    .pin_bit_mask = BIT(pin),          /*!< GPIO pin: set with bit mask, each bit maps to a GPIO */
    .mode = GPIO_MODE_OUTPUT,          /*!< GPIO mode: set input/output mode                     */
    .pull_up_en = GPIO_PULLUP_DISABLE, /*!< GPIO pull-up                                         */
    .pull_down_en = GPIO_PULLDOWN_DISABLE, /*!< GPIO pull-down */
    .intr_type = GPIO_INTR_DISABLE /*!< GPIO interrupt type                                  */
  };
  gpio_config(&cfg);
  // Initially set to 1
  // TODO: on, off level detection for cross relay model
  relay_ctx_t ctx = {
    .queue = xQueueCreate(2, sizeof(relay_msg_t)),
    .task = NULL, // set task handle to null
    .pin = pin,
    .on_level = 0,
    .off_level = 1,
    .on = false
  };
  gpio_set_level(pin, ctx.off_level);

  memcpy(ctx_out, &ctx, sizeof(ctx));
  ESP_LOGI(TAG, "Relay init port=%s pin=%d", port->port_name, pin);
  return ESP_OK;
}

static void relay_deinit(void *ctx) { (void)ctx; }

static esp_err_t relay_controller(void *ctxv, const cJSON *paramsJson, cJSON **out_result) {
  if (!paramsJson) return ESP_ERR_INVALID_ARG;
  if (!ctxv) {
    ESP_LOGE(TAG, "Context is null");
    return ESP_ERR_INVALID_STATE;
  }

  relay_ctx_t *ctx = (relay_ctx_t *)ctxv;
  if (!ctx->task) {
    xTaskCreate(relay_task, "relay_task", 2048, ctx, 5, &ctx->task);
  }

  cJSON *out = NULL;
  if (!paramsJson) return ESP_ERR_INVALID_ARG;
  if (!validate_params_json(params_desc, paramsJson, &out)) {
    if (out && out_result) *out_result = out;
    return ESP_ERR_INVALID_ARG;
  }

  bool on = ctx->on;
  if (json_get_bool(paramsJson, "enable", &on)) {
    if (ctx->on != on) {
      if (out_result && relay_send(ctx->queue, on ? RELAY_CMD_ON : RELAY_CMD_OFF, 0) == pdTRUE) {
        ctx->on = on;
        *out_result = build_success_fmt("turned %s relay", on ? "on" : "off");
      } else {
        *out_result = build_error_fmt("relay is busy");
      }
      return ESP_OK;
    }
  }

  double duration_sec = 0;
  if (json_get_number(paramsJson, "duration_sec", &duration_sec)) {
    uint32_t duration_ms = (uint32_t)duration_sec * 1000;
    if (out_result && relay_send(ctx->queue, RELAY_CMD_PULSE, duration_ms) == pdTRUE) {
      *out_result = build_success_fmt("activated relay for %.2lfs", duration_sec);
    } else {
      *out_result = build_error_fmt("relay is busy");
    }
    return ESP_OK;
  }

  if (out_result) *out_result = build_error_fmt("no changed");
  return ESP_OK;
}

const sn_driver_desc_t relay_driver = {
  .name = "relay"
          "_drv",
  .supported_types = relay_types,
  .priority = 50,
  .probe = relay_probe,
  .init = relay_init,
  .deinit = relay_deinit,
  .read_multi = NULL,
  .control = relay_controller,
  .command_desc = &schema
};
