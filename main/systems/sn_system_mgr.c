#include "sn_system_mgr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define SN_SYSTEM_MAX_QUEUE 16

static QueueHandle_t s_config_queue = NULL;
static bool s_system_has_error = false;
static const char *TAG = "sn-sysmgr";

void sn_sysmgr_init_self() {
  s_config_queue = xQueueCreate(SN_SYSTEM_MAX_QUEUE, sizeof(sys_config_t));
}

void sn_sysmgr_init_all() {
  sys_config_t config;
  while (xQueueReceive(s_config_queue, &config, pdMS_TO_TICKS(1000)) == pdTRUE) {
    ESP_LOGW(TAG, ">>>>>>>>>>>>>>>>>> Inializing %s", config.name);
    esp_err_t ret = config.init_cb(config.init_args);
    if (ret == ESP_OK) {
      ESP_LOGW(TAG, "<<<<<<<<<<<<<<<<<< Done");
    } else {
      ESP_LOGE(TAG, "<<<<<<<<<<<<<<<<<< Error");
      s_system_has_error = true;
      break;
    }
  }

  if (sn_sysmgr_is_valid()) {
    ESP_LOGI(TAG, "Systems initialization success.\n");
  } else {
    ESP_LOGE(TAG, "System encountered an error.\n");
  }
}

void sn_sysmgr_register(sys_config_t *config) {
  assert(config && "system config must not be null");
  assert(config->init_cb && "system must have an init callback");
  assert(config->name && "system must have a name");

  if (xQueueSend(s_config_queue, config, pdMS_TO_TICKS(1000)) != pdPASS) {
    ESP_LOGE(__func__, "system queue is full");
  }
}

bool sn_sysmgr_is_valid() { return !s_system_has_error; }

void sn_sysmgr_shutdown_all() {}
