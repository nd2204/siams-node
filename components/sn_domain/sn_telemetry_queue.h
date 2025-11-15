#ifndef SN_TELEMETRY_QUEUE_H
#define SN_TELEMETRY_QUEUE_H

#include "esp_err.h"
#include "freertos/idf_additions.h"
#include "sn_driver/sensor.h"
#include <string.h>

esp_err_t register_consumer(QueueHandle_t q);
void distribute_reading(sn_sensor_reading_t *reading);

#endif // !SN_TELEMETRY_QUEUE_H
