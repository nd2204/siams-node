#include "sn_devmgr.h"
#include "sn_ui.h"
#include "sn_wifi.h"

#include "nvs_flash.h"
#include "sdkconfig.h"
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/param.h>

#include "dht.h"
#include "soc/gpio_num.h"

void read_dht_task() {
  float temperature = 0.0f, humidity = 0.0f;

  // Read temperature and humidity
  while (1) {
    if (dht_read_float_data(DHT_TYPE_DHT11, GPIO_NUM_23, &humidity,
                            &temperature) == ESP_OK) {
      printf("Temperature: %.1f°C, Humidity: %.1f%%\n", temperature, humidity);
    } else {
      printf("Failed to read data from DHT sensor.\n");
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS); // Delay for 2 seconds
  }
}

void app_main(void) {
  sn_devmgr_init();
  lvgl_task();

  {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {
      /* If you only want to open more logs in the wifi module, you need to make
       * the max level greater than the default level, and call
       * esp_log_level_set() before esp_wifi_init() to improve the log level of
       * the wifi module. */
      esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }

    sn_wifi_init();
  }

  sn_wifi_connect(CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);

  read_dht_task();
  // xTaskCreate(read_dht_task, "dht-task", 512, NULL, 2, NULL);
}
