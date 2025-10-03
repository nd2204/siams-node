#include "sn_sensors.h"
#include "esp_timer.h"
#include "sn_devmgr.h"

#include "bh1750.h"
#include "dht.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <string.h>

#define SAMPLE_QUEUE_LEN 64

static QueueHandle_t s_sample_queue = NULL;

esp_err_t sn_sensors_init() {
  s_sample_queue = xQueueCreate(SAMPLE_QUEUE_LEN, sizeof(sensor_sample_t));
  return ESP_OK;
}

static void sn_sensors_enqueue_sample(sensor_sample_t *sample) {
  if (!sample) return;
  if (xQueueSend(s_sample_queue, sample, pdMS_TO_TICKS(100)) != pdPASS) {
    ESP_LOGW(__func__, "sample_queue full; dropped sample of sensor: %s", sample->name);
  }
}

void read_bh1750_task(void *pvParams) {
  static const char *TAG = __func__;
  bh1750_handle_t bh1750 = NULL;
  esp_err_t ret = ESP_OK;

  if (!s_sample_queue) {
    ESP_LOGE(TAG, "sample queue is not initialized");
    return;
  }

  ret = bh1750_create(sn_devmgr_get_i2c_master_bus_handle(), BH1750_I2C_ADDRESS_DEFAULT, &bh1750);
  if (ret != ESP_OK) {
    if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "cannot find slave address %.2x. quitting task", BH1750_I2C_ADDRESS_DEFAULT);
    } else {
      ESP_ERROR_CHECK(ret);
    }
    return;
  }

  ret = bh1750_power_on(bh1750);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "power on bh1750 failed");
    return;
  }

  float lux = 0.0f;

  for (int i = 0; i < 10; i++) {
    ESP_ERROR_CHECK(bh1750_set_measure_mode(bh1750, BH1750_ONETIME_1LX_RES));
    vTaskDelay(100); // Wait for measurement to be done

    SemaphoreHandle_t i2c_mutex = sn_devmgr_get_i2c_master_bus_mutex();
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
      bh1750_get_data(bh1750, &lux);
      printf("Lux: %.2f\n", lux);
      xSemaphoreGive(i2c_mutex);
    }
  }

  bh1750_delete(bh1750);
}

void read_dht_task(void *pvParams) {
  static const char *TAG = "read-dht-task";
  static const char *SENSOR_NAME = "dht11";

  if (!s_sample_queue) {
    ESP_LOGE(TAG, "sample queue is not initialized, task returned");
    return;
  }

  sensor_sample_t temp_sample, humi_sample;
  memset(&temp_sample, 0, sizeof(sensor_sample_t));
  strncpy(temp_sample.name, SENSOR_NAME, 6);
  temp_sample.unit = SU_CELCIUS;
  temp_sample.type = ST_TEMP;
  memset(&humi_sample, 0, sizeof(sensor_sample_t));
  strncpy(humi_sample.name, SENSOR_NAME, 6);
  temp_sample.type = ST_HUMI;
  humi_sample.unit = SU_PERCENTAGE;

  float temperature = 0.0f, humidity = 0.0f;
  int64_t timestamp = 0;

  // Read temperature and humidity
  while (1) {
    if (dht_read_float_data(DHT_TYPE_DHT11, GPIO_NUM_23, &humidity, &temperature) == ESP_OK) {
      timestamp = esp_timer_get_time() / 1000;
      temp_sample.value = temperature;
      temp_sample.ts_ms = timestamp;
      humi_sample.value = humidity;
      humi_sample.ts_ms = timestamp;
      sn_sensors_enqueue_sample(&temp_sample);
      sn_sensors_enqueue_sample(&humi_sample);
      printf("Temperature: %.1f°C, Humidity: %.1f%%\n", temperature, humidity);
    } else {
      printf("Failed to read data from DHT sensor.\n");
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS); // Delay for 2 seconds
  }
}
