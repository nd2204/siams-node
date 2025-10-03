#ifndef SN_SENSORS_H
#define SN_SENSORS_H

#include "esp_err.h"
#include <stdint.h>

// clang-format off
typedef enum {
  SU_CELCIUS = 0,
  SU_PERCENTAGE,
  SU_LUX,
} sensor_unit_e;

typedef enum {
  ST_TEMP = 0,
  ST_HUMI,
  ST_SOIL_HUMI,
  ST_LUMINOUS
} sample_type_e;
// clang-format on

typedef struct {
  char name[20];
  sensor_unit_e unit;
  sample_type_e type;
  double value;
  int64_t ts_ms;
} sensor_sample_t;

esp_err_t sn_sensors_init();

void read_bh1750_task(void *pvParams);

void read_dht_task(void *pvParams);

#endif // SN_SENSOR_T_H
