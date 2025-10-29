#pragma once

#include "freertos/idf_additions.h"
#include "sn_driver/driver_desc.h"
#include <string.h>

#define DRIVERS(X)                                                                                 \
  X(dht)                                                                                           \
  X(led)                                                                                           \
  X(soil_moisture)                                                                                 \
  X(light_intensity)

#define FORWARD_DECLARE_CTX_DRIVER_EXTERN(DRV_NAME)                                                \
  struct DRV_NAME##_ctx_s;                                                                         \
  typedef struct DRV_NAME##_ctx_s DRV_NAME##_ctx_t;                                                \
  extern const sn_driver_desc_t DRV_NAME##_driver;

extern bool g_adc1_initialized;
extern bool g_adc2_initialized;

// the driver source should define struct <drv_name>_ctx_t and const sn_driver_desc_t
// <drv_name>_driver
DRIVERS(FORWARD_DECLARE_CTX_DRIVER_EXTERN);
