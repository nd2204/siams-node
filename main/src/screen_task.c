#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "sn_driver.h"
#include "sn_driver/driver_inst.h"
#include "sn_driver/port_desc.h"
#include "sn_driver/sensor.h"
#include "sn_driver_registry.h"
#include "sn_telemetry_queue.h"

const char *TAG = "SCREEN_TASK";

void screen_task(void *pvParmas) {
  const sn_device_port_desc_t *screen_desc = find_device_by_driver_name("ssd1306");
  sn_device_instance_t *screen_inst = find_instance_by_name(screen_desc->port_name);
  if (!screen_desc || !screen_inst) {
    vTaskDelete(NULL);
  }

  QueueHandle_t telemetry_queue = xQueueCreate(6, sizeof(sn_sensor_reading_t));
  register_consumer(telemetry_queue);
  sn_sensor_reading_t reading;

  for (;;) {
    if (xQueueReceive(telemetry_queue, &reading, portMAX_DELAY) == pdTRUE) {
    }
  }
}
