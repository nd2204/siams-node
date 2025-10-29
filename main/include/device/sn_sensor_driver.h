#ifndef SN_SENSOR_DRIVER_H
#define SN_SENSOR_DRIVER_H

#include "esp_err.h"
#include <stdbool.h>

typedef struct {
  const char *name;
  esp_err_t (*init)(void);
  esp_err_t (*read)(float *out_value);
  bool available;
} sensor_driver_t;

#endif // !SN_SENSOR_DRIVER_H
