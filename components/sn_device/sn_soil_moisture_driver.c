#include "forward.h"
#include "hal/adc_types.h"
#include "sn_adc_helper.h"
#include "sn_driver/sensor.h"
#include "sn_driver_registry.h"

#include "esp_log.h"
#include "soc/gpio_num.h"
#include <stdbool.h>
#include <string.h>
#include <time.h>

static const char *TAG = "SN_SOIL_MOISTURE_DRIVER";
static const char *soil_moisture_types[] = {"soil_adc", NULL};

struct soil_moisture_ctx_s {
  adc1_channel_t channel;
  local_id_t sensor_id;
  uint32_t dry_mv;
  uint32_t wet_mv;
};

static esp_err_t soil_moisture_init(
  const sn_device_port_desc_t *port, void *ctx_out, size_t ctx_size
) {
  if (!port
      || !ctx_out
      || ctx_size < sizeof(soil_moisture_ctx_t)
      || port->desc.s.usage_type != PUT_GPIO
      || port->drv_type != DRIVER_TYPE_SENSOR)
    return ESP_ERR_INVALID_ARG;

  gpio_num_t pin = port->desc.s.usage.gpio.pin;

  // setup context
  soil_moisture_ctx_t ctx = {
    .channel = adc_helper_pin_to_channel(pin),
    .sensor_id = pm_get_local_id(port->desc.s.measurements, ST_MOISTURE),
    .dry_mv = 3300,
    .wet_mv = 1000 // Default calibration
  };

  if (ctx.sensor_id == INVALID_LOCAL_ID) {
    ESP_LOGE(TAG, "No measurement of type %s found in map", get_sensor_type_str(ST_MOISTURE));
    return ESP_ERR_INVALID_ARG;
  }
  if (ctx.channel == ADC1_CHANNEL_MAX) {
    ESP_LOGE(TAG, "GPIO pin=%d not supported for driver=%s", pin, port->drv_name);
    return ESP_ERR_NOT_SUPPORTED;
  }

  // configure attenuation (we choose 12dB for wider range)
  adc_helper_config_channel_atten(ctx.channel, ADC_ATTEN_DB_12);

  memcpy(ctx_out, &ctx, sizeof(ctx));

  return ESP_OK;
}

static void soil_moisture_deinit(void *ctx) { (void)ctx; }

static bool soil_moisture_probe(const sn_device_port_desc_t *port) {
  if (!port) return false;
  return (port->desc.s.usage.gpio.pin >= 0);
}

static esp_err_t soil_moisture_read_multi(
  void *ctxv, sn_sensor_reading_t *out_buf, int max_out, int *out_count
) {
  if (!ctxv || !out_buf) return ESP_ERR_INVALID_ARG;

  time_t now;

  soil_moisture_ctx_t *c = (soil_moisture_ctx_t *)ctxv;
  int raw = adc_helper_read_raw_avg(c->channel, 4);
  if (raw < 0) return ESP_FAIL;
  uint32_t mv = adc_helper_raw_to_mv(c->channel, raw);

  // clamp and map
  uint32_t dry = c->dry_mv;
  uint32_t wet = c->wet_mv;
  if (dry == wet) return ESP_ERR_INVALID_STATE;

  time(&now);
  out_buf[0].local_id = c->sensor_id;
  out_buf[0].ts = now;
  if (mv >= dry) {
    // more voltage -> dry in many circuits
    out_buf[0].value = 0.0f;
  } else if (mv <= wet) {
    out_buf[0].value = 100.0f;
  } else {
    // linear scale (wet..dry -> 100..0)
    float pct = ((float)(dry - mv) / (float)(dry - wet)) * 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    out_buf[0].value = pct;
  }
  *out_count = 1;
  return ESP_OK;
}

const sn_driver_desc_t soil_moisture_driver = {
  .name = "soil_moisture_drv",
  .supported_types = soil_moisture_types,
  .priority = 100,
  .probe = soil_moisture_probe,
  .init = soil_moisture_init,
  .read_multi = soil_moisture_read_multi,
  .control = NULL,
  .deinit = soil_moisture_deinit,
  .command_desc = NULL
};
