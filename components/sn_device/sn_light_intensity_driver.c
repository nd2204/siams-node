#include "esp_err.h"
#include "hal/adc_types.h"
#include "sn_adc_helper.h"
#include "sn_driver/sensor.h"
#include "sn_driver_registry.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "sn_sntp.h"
#include "soc/gpio_num.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "SN_LIGHT_INTENSITY_DRIVER";
static const char *light_intensity_sensor_types[] = {"lm393", NULL};

struct light_intensity_ctx_s {
  adc1_channel_t channel;
  local_id_t sensor_id;
  float A; // calibration: # max lux in bright conditions
  float B; // calibration: # exponential decay rate
  uint32_t dark_mv;
  uint32_t bright_mv;
};

static esp_err_t light_intensity_sensor_init(
  const sn_device_port_desc_t *port, void *ctx_out, size_t ctx_size
) {
  if (!port
      || !ctx_out
      || ctx_size < sizeof(light_intensity_ctx_t)
      || port->desc.s.usage_type != PUT_GPIO
      || port->drv_type != DRIVER_TYPE_SENSOR)
    return ESP_ERR_INVALID_ARG;

  gpio_num_t pin = port->desc.s.usage.gpio.pin;

  light_intensity_ctx_t ctx = {
    .channel = adc_helper_pin_to_channel(pin),
    .sensor_id = pm_get_local_id(port->desc.s.measurements, ST_LIGHT_INTENSITY),
    .A = 8038.29f,
    .B = -0.1893f,
    .dark_mv = 100,
    .bright_mv = 3000
  };

  if (ctx.sensor_id == 0) {
    ESP_LOGE(
      TAG, "No measurement of type %s found in map", get_sensor_type_str(ST_LIGHT_INTENSITY)
    );
    return ESP_ERR_INVALID_ARG;
  }

  if (ctx.channel == ADC1_CHANNEL_MAX) {
    ESP_LOGE(TAG, "GPIO pin=%d not supported for driver=%s", pin, port->drv_name);
    return ESP_ERR_NOT_SUPPORTED;
  }

  adc_helper_config_channel_atten(ctx.channel, ADC_ATTEN_DB_12);

  memcpy(ctx_out, &ctx, sizeof(ctx));

  return ESP_OK;
}

static void light_intensity_sensor_deinit(void *ctx) { (void)ctx; }

static bool light_intensity_sensor_probe(const sn_device_port_desc_t *port) {
  if (!port) return false;
  return (port->desc.s.usage.gpio.pin >= 0);
}

static esp_err_t light_intensity_sensor_read_multi(
  void *ctxv, sn_sensor_reading_t *out_buf, int max_out, int *out_count
) {
  if (!ctxv || !out_buf) return ESP_ERR_INVALID_ARG;
  light_intensity_ctx_t *c = (light_intensity_ctx_t *)ctxv;
  int raw = adc_helper_read_raw_avg(c->channel, 4);
  if (raw < 0) return ESP_FAIL;
  uint32_t mv = adc_helper_raw_to_mv(c->channel, raw);

  uint32_t dark = c->dark_mv;
  uint32_t bright = c->bright_mv;
  if (bright == dark) return ESP_ERR_INVALID_STATE;

  float D = 0.0f;
  if (mv <= dark) {
    D = 0.0f;
  } else if (mv >= bright) {
    D = 100.0f;
  } else {
    D = ((float)(mv - dark) / (float)(bright - dark)) * 100.0f;
    if (D < 0.0f) D = 0.0f;
    if (D > 100.0f) D = 100.0f;
  }

  out_buf[0].local_id = c->sensor_id;
  out_buf[0].value = c->A * exp(c->B * D);
  out_buf[0].ts = sn_get_unix_timestamp_ms();
  *out_count = 1;
  return ESP_OK;
}

const sn_driver_desc_t light_intensity_driver = {
  .name = "light_intensity_sensor_drv",
  .supported_types = light_intensity_sensor_types,
  .priority = 100,
  .probe = light_intensity_sensor_probe,
  .init = light_intensity_sensor_init,
  .read_multi = light_intensity_sensor_read_multi,
  .control = NULL,
  .deinit = light_intensity_sensor_deinit,
  .command_desc = NULL
};
