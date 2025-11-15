#include "sn_telemetry_queue.h"
#include "esp_log.h"

#define MAX_CONSUMERS 16
static QueueHandle_t consumer_queues[MAX_CONSUMERS];
static int consumer_count = 0;

esp_err_t register_consumer(QueueHandle_t q) {
  if (consumer_count > MAX_CONSUMERS) {
    return ESP_ERR_NO_MEM;
  }
  consumer_queues[consumer_count++] = q;
  return ESP_OK;
}

void distribute_reading(sn_sensor_reading_t *reading) {
  sn_sensor_reading_t data;
  memcpy(&data, reading, sizeof(sn_sensor_reading_t));
  for (int i = 0; i < consumer_count; i++) {
    xQueueSend(consumer_queues[i], &data, 0);
  }
}
