#pragma once

#include "freertos/idf_additions.h"
#include "sn_driver/driver_desc.h"
#include <string.h>

#define DRIVERS(X)                                                                                 \
  X(dht)                                                                                           \
  X(led)                                                                                           \
  X(relay)                                                                                         \
  X(soil_moisture)                                                                                 \
  X(light_intensity)                                                                               \
  X(screen_i2c)

#define FORWARD_DECLARE_CTX_DRIVER_EXTERN(DRV_NAME)                                                \
  struct DRV_NAME##_ctx_s;                                                                         \
  typedef struct DRV_NAME##_ctx_s DRV_NAME##_ctx_t;                                                \
  extern const sn_driver_desc_t DRV_NAME##_driver;

// the driver source should define struct <drv_name>_ctx_t and const sn_driver_desc_t
// <drv_name>_driver
DRIVERS(FORWARD_DECLARE_CTX_DRIVER_EXTERN);

// Expands this template using lsp
#define DEFINE_DRIVER_TEMPLATE(DRV_NAME)                                                           \
  struct DRV_NAME##_ctx_s {};                                                                      \
  static const char *TAG = #DRV_NAME;                                                              \
  static const char *DRV_NAME##_types[] = {"", NULL};                                              \
  static const sn_param_desc_t params_desc[] = {{.name = NULL}};                                   \
  static const sn_command_desc_t schema = {                                                        \
    .action = NULL,                                                                                \
    .params = params_desc,                                                                         \
    .params_count = ((sizeof(params_desc) / sizeof(sn_param_desc_t)) - 1)                          \
  };                                                                                               \
  static bool DRV_NAME##_probe(const sn_device_port_desc_t *port) {}                               \
  static esp_err_t DRV_NAME##_init(                                                                \
    const sn_device_port_desc_t *desc, void *ctx_out, size_t ctx_size                              \
  ) {                                                                                              \
    return ESP_OK;                                                                                 \
  }                                                                                                \
  static void DRV_NAME##_deinit(void *ctx) { (void)ctx; }                                          \
  static esp_err_t DRV_NAME##_read_multi(                                                          \
    void *ctxv, sn_sensor_reading_t *out_buf, int max_out, int *out_count                          \
  ) {                                                                                              \
    return ESP_OK;                                                                                 \
  }                                                                                                \
  static esp_err_t DRV_NAME##_controller(                                                          \
    void *ctxv, const cJSON *paramsJson, cJSON **out_result                                        \
  ) {                                                                                              \
    return ESP_OK;                                                                                 \
  }                                                                                                \
  const sn_driver_desc_t DRV_NAME##_driver = {                                                     \
    .name = #DRV_NAME##"_drv",                                                                     \
    .supported_types = DRV_NAME##_types,                                                           \
    .priority = 50,                                                                                \
    .probe = DRV_NAME##_probe,                                                                     \
    .init = DRV_NAME##_init,                                                                       \
    .deinit = DRV_NAME##_deinit,                                                                   \
    .read_multi = DRV_NAME##_read_multi,                                                           \
    .control = DRV_NAME##_controller,                                                              \
    .command_desc = &schema                                                                        \
  };
