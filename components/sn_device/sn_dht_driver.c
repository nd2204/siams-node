#include "esp_err.h"
#include "sn_driver/sensor.h"
#include "sn_driver_registry.h"

#include "dht.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "sn_json.h"
#include "sn_sntp.h"
#include "soc/gpio_num.h"
#include <stdbool.h>
#include <string.h>
#include <time.h>

static const char *TAG = "SN_DHT_DRIVER";
static const char *dht_types[] = {"dht11",  "dht21",  "dht22",   "dht",
                                  "AM2301", "AM2302", "SiI7021", NULL};

struct dht_ctx_s {
  gpio_num_t pin;
  int last_read_ms;
  float last_temp;
  float last_hum;
  dht_sensor_type_t dht_type;
  local_id_t temperature_id;
  local_id_t humidity_id;
  bool cached;
};

static esp_err_t dht_init(const sn_device_port_desc_t *port, void *ctx_out, size_t ctx_size) {
  if (!port || !ctx_out || ctx_size < sizeof(dht_ctx_t) || port->desc.s.usage_type != PUT_GPIO) {
    return ESP_ERR_INVALID_ARG;
  }
  gpio_num_t _pin = port->desc.s.usage.gpio.pin;

  // clang-format off
  dht_ctx_t tmp = {
    .pin = _pin,
    .last_read_ms = 0,
    .last_temp = 0.0f,
    .last_hum = 0.0f,
    .temperature_id = 0,
    .humidity_id = 0,
    .dht_type = DHT_TYPE_AM2301,
    .cached = 0
  };
  // clang-format on

  if (strncmp(port->drv_name, "dht11", 5) == 0) {
    tmp.dht_type = DHT_TYPE_DHT11;
  } else if (strncmp(port->drv_name, "Si7021", 5) == 0) {
    tmp.dht_type = DHT_TYPE_SI7021;
  }

  // find and assign local_id_t for dht context (temp and humi)
  tmp.temperature_id = pm_get_local_id(port->desc.s.measurements, ST_TEMPERATURE);
  tmp.humidity_id = pm_get_local_id(port->desc.s.measurements, ST_HUMIDITY);

  if (tmp.temperature_id == 0 || tmp.humidity_id == 0) return ESP_ERR_INVALID_ARG;
  memcpy(ctx_out, &tmp, sizeof(tmp));

  ESP_LOGI(TAG, "DHT init port=%s pin=%d", port->port_name, _pin);
  return ESP_OK;
}

static void dht_deinit(void *ctx) { (void)ctx; }

static bool dht_probe(const sn_device_port_desc_t *port) {
  if (!port) return false;
  // light probe: if pin >=0 assume possible
  return (port->desc.s.usage.gpio.pin >= 0);
}

static esp_err_t dht_read_multi(
  void *ctxv, sn_sensor_reading_t *out_buf, int max_out, int *out_count
) {
  if (!ctxv || !out_buf || max_out < 2 || !out_count) return ESP_ERR_INVALID_ARG;
  dht_ctx_t *ctx = (dht_ctx_t *)ctxv;
  uint64_t now_ms = esp_timer_get_time() / 1000ULL;

  // use cache to serve multiple measurements without re-reading hardware often
  if (!ctx->cached || (now_ms - ctx->last_read_ms) > 3000) {
    float t = 0.0f, h = 0.0f;
    esp_err_t r = dht_read_float_data(ctx->dht_type, ctx->pin, &h, &t);
    if (r != ESP_OK) {
      ctx->cached = false;
      return r;
    }
    ctx->last_temp = t;
    ctx->last_hum = h;
    ctx->last_read_ms = (int)now_ms;
    ctx->cached = true;
  }

  unsigned long long now = sn_get_unix_timestamp_ms();

  out_buf[0].local_id = ctx->temperature_id;
  out_buf[0].value = ctx->last_temp;
  out_buf[0].ts = now;

  out_buf[1].local_id = ctx->humidity_id;
  out_buf[1].value = ctx->last_hum;
  out_buf[1].ts = now;

  *out_count = 2;

  return ESP_OK;
}

const sn_driver_desc_t dht_driver = {
  .name = "dht_drv",
  .supported_types = dht_types,
  .priority = 100,
  .probe = dht_probe,
  .init = dht_init,
  .read_multi = dht_read_multi,
  .control = NULL,
  .deinit = dht_deinit,
  .command_desc = NULL
};
