#include "esp_timer.h"
#include "sn_driver.h"
#include "sn_mqtt_manager.h"
#include "sn_telemetry_queue.h"
#include "sn_topic.h"

static const char *TAG = "SENSOR_POLL_TASK";

void sensor_poll_task(void *pvParam) {
  size_t len = sn_driver_get_instance_len();
  sn_device_instance_t *instances = sn_driver_get_device_instances();

  sn_sensor_reading_t readings[4];
  for (;;) {
    FOR_EACH_SENSOR_INSTANCE(it, instances, len) {
      uint64_t now_ms = esp_timer_get_time() / 1000ULL;
      // check schedule
      if ((now_ms - it->last_read_ms) < it->interval_ms) continue;
      if (!it->online || !it->driver || !it->driver->read_multi) continue;

      int outcount = 0;
      esp_err_t r = it->driver->read_multi((void *)&it->ctx, readings, sizeof(readings), &outcount);
      if (r == ESP_OK && outcount > 0) {
        it->last_read_ms = now_ms;
        it->consecutive_failures = 0;

        for (int i = 0; i < outcount; i++) {
          // notify subscriber
          distribute_reading(&readings[i]);
          // send reading to publisher task
          publish_telemetry(&readings[i]);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
      } else {
        it->consecutive_failures++;
        ESP_LOGW(
          TAG, "Read failed for port=%s driver=%s rc=%d fails=%d", it->port->port_name,
          it->driver ? it->driver->name : "(null)", r, it->consecutive_failures
        );
        if (it->consecutive_failures >= 3) {
          it->online = false;
          ESP_LOGW(TAG, "Marking port %s offline after consecutive failures", it->port->port_name);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
