#ifndef SN_SENSOR_SAMPLE_H
#define SN_SENSOR_SAMPLE_H

// clang-format off
#include "sn_device.h"
#include <stdint.h>

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
  local_id_t id;
  double value;
} sensor_sample_t;

#endif // !SN_SENSOR_SAMPLE_H
