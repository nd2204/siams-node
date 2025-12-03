#include "esp_random.h"
#include "sn_capability.h"
#include "sn_error.h"
// mqtt
#include "sn_mqtt_router.h"
#include "sn_mqtt_manager.h"
// internet and time
#include "sn_inet.h"
#include "sn_rules/sn_rule_engine.h"
#include "sn_security.h"
#include "sn_sntp.h"
// persistence
#include "sn_storage.h"
// drivers
#include "sn_driver.h"
#include "sn_driver_registry.h"

// esp-idf framework libs
#include <esp_log.h>
#include "cJSON.h"
#include "sdkconfig.h"
#include "sn_topic.h"
#include "esp_err.h"
#include <freertos/FreeRTOS.h>
#include <sys/time.h>

#define WIFI_SSID CONFIG_ESP_WIFI_SSID
#define WIFI_PASS CONFIG_ESP_WIFI_PASSWORD

static const char *TAG = "MAIN";

extern void sensor_poll_task(void *pvParam);
extern void status_poll_task(void *pvParam);

static esp_err_t init_device_config();
static esp_err_t init_drivers(void);
static esp_err_t start_mqtt_registration_verification();

// mqtt command callback
static void on_command_msg(const char *topic, const char *payload) {
  ESP_LOGI("CMD", "Command received: %s", payload);
  cJSON *out = NULL;
  dispatch_command(payload, &out);
  if (out) {
    char *payload_str = cJSON_PrintUnformatted(out);
    const sn_mqtt_topic_cache_t *cache = sn_mqtt_topic_cache_get();
    sn_mqtt_publish_enqueue(cache->command_ack_topic, payload_str, 1, false);
    cJSON_free(payload_str);
    cJSON_Delete(out);
  }
}
void app_main(void) {
  GOTO_IF_ESP_ERROR(end, init_drivers());
  // Init modules
  GOTO_IF_ESP_ERROR(end, sn_storage_init(NULL));
  GOTO_IF_ESP_ERROR(end, sn_inet_init(NULL));
  // Connect to the internet using wifi this will block and wait for the connection
  GOTO_IF_ESP_ERROR(end, sn_inet_wifi_connect(WIFI_SSID, WIFI_PASS));
  GOTO_IF_FALSE_MSG(end, sn_inet_is_connected(), TAG, "Network is not available");
  // Init sntp server and sync time
  GOTO_IF_ESP_ERROR(end, sn_init_sntp());
  GOTO_IF_ESP_ERROR(end, init_device_config());

  // Register the device to the backend for deviceId
  // Verify the existing deviceId otherwise
  GOTO_IF_ESP_ERROR(end, start_mqtt_registration_verification());

  // create mqtt client
  const sn_mqtt_topic_cache_t *cache = sn_mqtt_topic_cache_get();
  // create lwt payload from timestamp
  cJSON *payload = sn_security_sign_and_wrap_payload(create_lwt_payload_json());
  char *payload_str = cJSON_PrintUnformatted(payload);
  // create config and initialize
  sn_mqtt_config_t conf =
    {.uri = CONFIG_MQTT_BROKER_URI, .lwt_topic = cache->status_topic, .lwt_payload = payload_str};
  sn_mqtt_init(&conf);
  // clean up
  cJSON_free(payload_str);
  cJSON_Delete(payload);

  sn_mqtt_start();
  sn_mqtt_router_subscriber_add(cache->command_topic, on_command_msg, 1);

  xTaskCreatePinnedToCore(sensor_poll_task, "sensor_poll_task", 4096, NULL, 4, NULL, 0);
  xTaskCreatePinnedToCore(status_poll_task, "status_task", 4096, NULL, 5, NULL, 1);
  xTaskCreatePinnedToCore(rule_engine_task, "rule_engine_task", 4096, NULL, 5, NULL, 1);
end:
  // vTaskDelay(pdMS_TO_TICKS(5000));
  // esp_restart();
  (void)0;
}

static esp_err_t init_device_config() {
  // WARN: We should remove cluster config and move to provisioning api
  // to avoid rebuilding

  // Init organization id
  if (CONFIG_ORG_ID[0] != '\0') sn_storage_set_org_id(CONFIG_ORG_ID);

  sn_mqtt_topic_context_t ctx = {.orgId = CONFIG_ORG_ID};
  if (sn_storage_get_device_id(ctx.deviceId, sizeof(ctx.deviceId)) != ESP_OK) {
    uint32_t r = esp_random();
    snprintf(ctx.deviceId, sizeof(ctx.deviceId), "temp-%08lx", r);
  }
  sn_mqtt_topic_cache_init(&ctx);
  print_topic_cache();

  // sn_storage_get_org_id(ctx.orgId, sizeof(ctx.orgId));
  // sn_storage_get_cluster_id(ctx.clusterId, sizeof(ctx.clusterId));
  // sn_storage_list_all();
  return ESP_OK;
}

extern esp_err_t register_device();
extern esp_err_t verify_device();
extern esp_err_t recover_device();

static esp_err_t start_mqtt_registration_verification() {
  char device_id[64];
  char device_secret[65];
  bool has_id = sn_storage_get_device_id(device_id, sizeof(device_id)) == ESP_OK;
  bool has_secret = sn_storage_get_device_secret(device_secret, sizeof(device_secret)) == ESP_OK;
  esp_err_t ok = ESP_OK;
  // Get device id from storage
  if (!has_id)
    ESP_LOGW(TAG, "device_id not found");
  else
    ESP_LOGI(TAG, "device_id found: %s", device_id);
  if (!has_secret)
    ESP_LOGW(TAG, "device_secret not found");
  else
    ESP_LOGI(TAG, "device_secret found: %s", device_secret);

  ok = !has_id ? (has_secret ? recover_device() : register_device()) :
                 (has_secret ? verify_device() : ESP_ERR_INVALID_STATE);

  if (ok != ESP_OK) {
    ESP_LOGE(TAG, "Failed with error: %d(%s). Restarting in 5s...", ok, esp_err_to_name(ok));
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
  }

  return ok;
}

static esp_err_t init_drivers(void) {
  esp_err_t err = ESP_OK;
#define X(driver) ESP_ERROR_CHECK_WITHOUT_ABORT(err = sn_driver_register(&driver##_driver));
  DRIVERS(X)
#undef X
  sn_driver_bind_all_ports(gDevicePorts, gDevicePortsLen);
  return err;
}
