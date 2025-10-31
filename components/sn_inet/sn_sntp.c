#include "sn_sntp.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include <sys/time.h>
#include <time.h>

static const char *TAG = "time_sync";

esp_err_t sn_init_sntp(void) {
  ESP_LOGI(TAG, "Initializing SNTP");

  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_init();

  // Wait for time to be set
  time_t now = 0;
  struct tm timeinfo = {0};
  int retry = 0;
  const int retry_count = 10;
  while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
    ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    time(&now);
    localtime_r(&now, &timeinfo);
  }

  if (timeinfo.tm_year < (2016 - 1900)) {
    ESP_LOGE(TAG, "Time sync failed");
    return ESP_FAIL;
  } else {
    ESP_LOGI(TAG, "Time synchronized");
    return ESP_OK;
  }
}

void sn_get_iso8601_timestamp(char *buffer, size_t buffer_size) {
  time_t now;
  struct tm timeinfo;
  struct timeval tv;

  gettimeofday(&tv, NULL); // for seconds + microseconds
  time(&now);
  gmtime_r(&now, &timeinfo); // use UTC for ISO8601 (Z suffix)

  // clang-format off
  // Format: YYYY-MM-DDTHH:MM:SS.123Z
  snprintf(
    buffer,
    buffer_size,
    "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec,
    tv.tv_usec / 1000
  );
}

