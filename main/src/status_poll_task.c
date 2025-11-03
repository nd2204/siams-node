#include "esp_log.h"
#include "esp_system.h"
#include <esp_log.h>
#include <esp_heap_caps.h>

static const char *TAG = "STATUS_POLL_TASK";

void print_memory_info() {
  ESP_LOGI(TAG, "Free heap size: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "Minimum free heap size: %d bytes", esp_get_minimum_free_heap_size());
  ESP_LOGI(TAG, "Free internal RAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  ESP_LOGI(
    TAG, "Minimum free internal RAM: %d bytes", heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)
  );
}
