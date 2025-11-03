#include "sn_driver.h"

void sensor_poll_task(void *pvParam) {
  size_t len = sn_driver_get_instance_len();
  sn_device_instance_t *instances = sn_driver_get_device_instances();

  char *reading_json_str = NULL;
  sn_sensor_reading_t readings[4];
  for (;;) {
    cJSON *reading_json = NULL;
    FOR_EACH_SENSOR_INSTANCE(it, instances, len) {
      int outcount = 0;
      if (it->online && it->driver->read_multi) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(
          it->driver->read_multi((void *)&it->ctx, readings, sizeof(readings), &outcount)
        );
      }
      for (int i = 0; i < outcount; i++) {
        reading_json = sensor_reading_to_json_obj(&readings[i]);
        if (reading_json) {
          reading_json_str = cJSON_PrintUnformatted(reading_json);
          puts(reading_json_str);
          if (reading_json_str) cJSON_free(reading_json_str);
          cJSON_Delete(reading_json);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}
