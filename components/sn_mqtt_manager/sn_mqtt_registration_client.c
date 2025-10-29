#include "sn_json.h"
#include "sn_storage.h"
#include "sn_topic.h"
#include "sn_mqtt_registration_client.h"

#include "cJSON.h"
#include "esp_random.h"
#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "SN_MQTT_REG_CLIENT";
static bool got_ack = false;
static esp_mqtt_client_handle_t reg_client = NULL;

void gen_temp_id(char *out, size_t len) {
  uint32_t r = esp_random();
  snprintf(out, len, "temp-%08lx", r);
}

// static cJSON *create_register_payload(const char *tempId) {
//   cJSON *root = cJSON_CreateObject();
//   cJSON_AddStringToObject(root, "tempId", tempId);
//   cJSON_AddStringToObject(root, "model", "ESP32-S3");
//   cJSON_AddStringToObject(root, "firmwareVersion", "1.0.0");
//   const device_capabilities_t capabilities = {
//     .sensors = gSensorCapabilities,
//     .actuators = gActuatorCapabilities,
//     .commands = gCommandCapabilities,
//     .slen = SENSOR_COUNT,
//     .alen = ACTUATOR_COUNT,
//     .clen = COMMAND_COUNT
//   };
//   cJSON *object = device_capabilities_to_json(&capabilities);
//   cJSON_AddItemToObject(root, "capabilities", object);
//   return root;
// }
//
// Callback for MQTT events
static void mqtt_event_handler(void *args, esp_event_base_t base, int32_t eid, void *edata) {
  esp_mqtt_event_handle_t event = edata;
  const char *payload = (const char *)args;

  if (!payload) {
    ESP_LOGE(TAG, "payload is null or empty");
    return;
  }

  switch ((esp_mqtt_event_id_t)eid) {
    case MQTT_EVENT_CONNECTED: {
      ESP_LOGI(TAG, "Connected to broker (registration mode)");
      // Subscribe to ack topic

      const sn_mqtt_topic_context_t *ctx = sn_mqtt_topic_cache_get_context();
      char ack_topic[256];
      snprintf(
        ack_topic, sizeof(ack_topic), TOPIC_REGISTER_ACK_FMT, ctx->orgId, ctx->clusterId,
        ctx->deviceId
      );
      esp_mqtt_client_subscribe(reg_client, ack_topic, 1);

      // Publish registration payload
      char reg_topic[256];
      snprintf(
        reg_topic, sizeof(reg_topic), TOPIC_REGISTER_FMT, ctx->orgId, ctx->clusterId, ctx->deviceId
      );

      esp_mqtt_client_publish(reg_client, reg_topic, payload, 0, 1, true);
      break;
    }
    case MQTT_EVENT_DATA: {
      char topic[128];
      snprintf(topic, event->topic_len + 1, "%.*s", event->topic_len, event->topic);
      char data[256];
      snprintf(data, event->data_len + 1, "%.*s", event->data_len, event->data);

      ESP_LOGI(TAG, "ACK received on %s: %s", topic, data);
      cJSON *json = cJSON_Parse(data);
      if (!json) break;
      const char *deviceId = NULL;
      if (json_get_string(json, "deviceId", &deviceId)) {
        sn_storage_set_device_id(deviceId);
        ESP_LOGI(TAG, "Device registered successfully: %s", deviceId);
        got_ack = true;
      }
      cJSON_Delete(json);
      break;
    }
    default:
      break;
  }
}

esp_err_t mqtt_registration_run(const char *broker_uri, const char *register_payload) {
  char mqtt_user[64];
  char mqtt_pass[64];

  esp_err_t err =
    sn_storage_get_credentials(mqtt_user, sizeof(mqtt_user), mqtt_pass, sizeof(mqtt_pass));
  if (err != ESP_OK) {
    mqtt_user[0] = '\0';
    mqtt_pass[0] = '\0';
  }

  esp_mqtt_client_config_t cfg = {
    .broker.address.uri = broker_uri,
    .credentials.username = mqtt_user[0] ? mqtt_user : NULL,
    .credentials.authentication.password = mqtt_pass[0] ? mqtt_user : NULL
  };

  reg_client = esp_mqtt_client_init(&cfg);
  esp_mqtt_client_register_event(
    reg_client, ESP_EVENT_ANY_ID, mqtt_event_handler, (void *)register_payload
  );
  esp_mqtt_client_start(reg_client);

  // Wait until ack received or timeout (15s)
  int retry = 0;
  int retry_count = 30;
  while (!got_ack && retry++ < retry_count) {
    ESP_LOGI(TAG, "Waiting for registration ack... (%02d/%d)", retry, retry_count);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  esp_mqtt_client_stop(reg_client);
  esp_mqtt_client_destroy(reg_client);

  if (got_ack)
    ESP_LOGI(TAG, "Registration complete, saved deviceId");
  else
    ESP_LOGW(TAG, "Registration timeout or failed");

  return got_ack ? ESP_OK : ESP_FAIL;
}
