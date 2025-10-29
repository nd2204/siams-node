#ifndef SN_SENSORS_H
#define SN_SENSORS_H

#include "esp_err.h"
#include <stdint.h>

esp_err_t sn_sensors_init(void *pvParams);

void read_bh1750_task(void *pvParams);

void read_dht_task(void *pvParams);

#endif // SN_SENSOR_T_H
