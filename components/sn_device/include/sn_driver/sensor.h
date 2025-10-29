#ifndef SN_SENSOR_DRIVER_H
#define SN_SENSOR_DRIVER_H

#include "esp_err.h"
#include "esp_log.h"
#include "forward.h"
#include <time.h>

#define SENSOR_TYPE(X)                                                                             \
  X(TEMPERATURE)                                                                                   \
  X(HUMIDITY)                                                                                      \
  X(MOISTURE)                                                                                      \
  X(LIGHT_INTENSITY)

typedef enum {
  ST_INVALID = -1,
#define TO_ENUM_FIELD(c) ST_##c,
  SENSOR_TYPE(TO_ENUM_FIELD)
#undef TO_ENUM_FIELD
    ST_MAX,
} sensor_type_e;

// We can assume that each measurement type is unique
// Because it would be weird if one sensor measure the same metric twice
typedef struct sn_sensor_reading_s {
  local_id_t local_id;
  float value;
  time_t ts;
} sn_sensor_reading_t;

typedef struct {
  const char *unit;   // "C", "%"
  sensor_type_e type; // measurement type: TEMPERATURE etc
  local_id_t local_id;
} sn_port_measurement_map_t;

// read_multi: fill measurements into out_buf (max_out entries), set out_count
typedef esp_err_t (*read_multi_fn_t)(
  void *ctx, sn_sensor_reading_t *outBuf, int maxOut, int *outCount
);

/* --------------------------------------------------------------------
 *  Creation helpers
 * ------------------------------------------------------------------*/

#define MEASUREMENT_MAP_ENTRY(ID, TYPE, UNIT) {.unit = UNIT, .type = TYPE, .local_id = ID}

#define MEASUREMENT_MAP_ENTRY_NULL() {.unit = NULL, .type = 0, .local_id = 0}

/* --------------------------------------------------------------------
 *  Iteration helpers
 * ------------------------------------------------------------------*/

#define FOR_EACH_MEASUREMENT(it, arr)                                                              \
  for (const sn_port_measurement_map_t *it = (arr); it && ((it)->unit || (it)->local_id > 0); ++it)

/* --------------------------------------------------------------------
 *  Utility functions for measurement array queries
 * ------------------------------------------------------------------*/

// Count valid entries (until name == NULL)
static inline const char *get_sensor_type_str(sensor_type_e type) {
  static const char *sensorTypeStr[] = {
#define STR(x) #x,
    SENSOR_TYPE(STR)
#undef STR
  };
  if (type > ST_INVALID && type < ST_MAX) {
    return sensorTypeStr[type];
  }
  return "Unknown sensor type";
}

// Count valid entries (until name == NULL)
static inline size_t pm_count(const sn_port_measurement_map_t *arr) {
  size_t n = 0;
  if (!arr) return 0;
  for (; arr[n].unit != NULL; ++n) {
  }
  return n;
}

// Find by measurement type
static inline const sn_port_measurement_map_t *pm_find_by_type(
  const sn_port_measurement_map_t *arr, sensor_type_e type
) {
  if (!arr) return NULL;
  FOR_EACH_MEASUREMENT(it, arr) {
    if (type == it->type) return it;
  }
  return NULL;
}

// Find by localId
static inline const sn_port_measurement_map_t *pm_find_by_local_id(
  const sn_port_measurement_map_t *arr, uint8_t local_id
) {
  if (!arr) return NULL;
  FOR_EACH_MEASUREMENT(it, arr) {
    if (it->local_id == local_id) return it;
  }
  return NULL;
}

// Get localId by measurement type (0 if not found)
static inline uint8_t pm_get_local_id(const sn_port_measurement_map_t *arr, sensor_type_e type) {
  const sn_port_measurement_map_t *m = pm_find_by_type(arr, type);
  return m ? m->local_id : 0;
}

// Get sensor type type by localId (-1 if not found)
static inline sensor_type_e pm_get_type_by_local_id(
  const sn_port_measurement_map_t *arr, uint8_t local_id
) {
  const sn_port_measurement_map_t *m = pm_find_by_local_id(arr, local_id);
  return m ? m->type : ST_INVALID;
}

/* --------------------------------------------------------------------
 *  Optional: pretty-print
 * ------------------------------------------------------------------*/
static inline void pm_print(const sn_port_measurement_map_t *arr, const char *tag) {
  if (!arr) return;
  if (!tag) tag = "MEAS";
  FOR_EACH_MEASUREMENT(m, arr) {
    ESP_LOGI(
      tag, "Measurement %-12s | unit=%-5s | localId=0x%02X", get_sensor_type_str(m->type),
      m->unit ? m->unit : "-", m->local_id
    );
  }
}

#endif // !SN_SENSOR_DRIVER_H
