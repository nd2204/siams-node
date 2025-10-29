#ifndef SN_SENSOR_REGISTRY_H
#define SN_SENSOR_REGISTRY_H

#include "sensors/sn_sensor_driver.h"
#include "sn_config.h"

#define MAX_SENSORS CONFIG_DEVICE_MAX_SENSORS_SUPPORTED

typedef struct {
  sensor_driver_t *sensors[MAX_SENSORS];
  int count;
} sensor_registry_t;

void sensor_registry_init(sensor_registry_t *registry);

void sensor_registry_register(sensor_registry_t *registry, sensor_driver_t *driver);

void sensor_registry_detect_all(sensor_registry_t *registry);

void sensor_registry_read_all(sensor_registry_t *registry);

#endif // !SN_SENSOR_REGISTRY_H
