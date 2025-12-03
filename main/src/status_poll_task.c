#include "esp_timer.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include "freertos/idf_additions.h"
#include "sn_inet.h"
#include "sn_mqtt_manager.h"
#include "sn_sntp.h"
#include "sn_topic.h"

static const char *TAG = "STATUS_POLL_TASK";

#ifndef STATUS_POLL_INTERVAL_MS
#define STATUS_POLL_INTERVAL_MS 8000 // 8 seconds
#endif

static inline float get_memory_usage(void) {
  size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
  size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
  if (total_heap == 0) return 0.0f;
  return 1.0f - ((float)free_heap / (float)total_heap);
}

// track CPU time
static uint64_t s_last_idle_time = 0;
static uint64_t s_last_total_time = 0;
static float s_last_cpu_usage = 0.0f;

static float get_cpu_usage(void) {
  // Approximation using idle task time deltas
  // (lightweight method; accurate enough for monitoring)
  uint64_t idle_time = esp_cpu_get_cycle_count();
  uint64_t total_time = esp_timer_get_time(); // microseconds

  if (s_last_total_time == 0) {
    s_last_idle_time = idle_time;
    s_last_total_time = total_time;
    return 0.0f;
  }

  uint64_t delta_idle = idle_time - s_last_idle_time;
  uint64_t delta_total = total_time - s_last_total_time;
  s_last_idle_time = idle_time;
  s_last_total_time = total_time;

  if (delta_total == 0) return s_last_cpu_usage;
  float usage = 1.0f - ((float)delta_idle / (float)delta_total);
  if (usage < 0) usage = 0;
  if (usage > 1) usage = 1;
  s_last_cpu_usage = usage;
  return usage;
}

static inline esp_err_t publish_status(const sn_status_reading_t *status) {
  if (!status) return ESP_ERR_INVALID_ARG;
  const char *topic = sn_mqtt_topic_cache_get()->status_topic;
  cJSON *json = cJSON_CreateObject();
  cJSON_AddNumberToObject(json, "cpu", status->cpu);
  cJSON_AddNumberToObject(json, "mem", status->mem);
  cJSON_AddNumberToObject(json, "wifi", status->wifi);
  cJSON_AddNumberToObject(json, "ts", status->ts);
  cJSON_AddBoolToObject(json, "online", true);
  return sn_mqtt_publish_json_payload_signed(json, topic, 0, false);
}

void status_poll_task(void *pvParams) {
  ESP_LOGI(TAG, "Status poll task started");
  sn_status_reading_t reading = {0};
  for (;;) {
    reading.mem = get_memory_usage();
    reading.cpu = get_cpu_usage();
    reading.wifi = sn_inet_get_wifi_rssi();
    reading.ts = sn_get_unix_timestamp_ms();
    publish_status(&reading);
    vTaskDelay(pdMS_TO_TICKS(STATUS_POLL_INTERVAL_MS));
  }
}
