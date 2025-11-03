#include "sn_json.h"
#include "sn_storage.h"
#include "sn_topic.h"
#include "sn_mqtt_verify_client.h"

#include "cJSON.h"
#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "SN_MQTT_VERIFY_CLIENT";
static bool got_ack = false;
static bool got_err_ack = false;
static esp_mqtt_client_handle_t reg_client = NULL;

// Callback for MQTT events
static void mqtt_event_handler(void *args, esp_event_base_t base, int32_t eid, void *edata) {
  esp_mqtt_event_handle_t event = edata;
  const char *payload = (const char *)args;
  const char *status_rx = NULL;

  if (!payload) {
    ESP_LOGE(TAG, "payload is null or empty");
    return;
  }

  switch ((esp_mqtt_event_id_t)eid) {
    case MQTT_EVENT_CONNECTED: {
      ESP_LOGI(TAG, "Connected to broker (verification mode)");
      // Subscribe to ack topic
      const sn_mqtt_topic_cache_t *cache = sn_mqtt_topic_cache_get();
      // Subscribe to verify ack
      esp_mqtt_client_subscribe(reg_client, cache->verify_ack_topic, 1);
      // Publish to verify
      esp_mqtt_client_publish(reg_client, cache->verify_topic, payload, 0, 1, false);
      break;
    }

    case MQTT_EVENT_DATA: {
      char data[256];
      snprintf(data, event->data_len + 1, "%.*s", event->data_len, event->data);
      char topic[128];
      snprintf(topic, event->topic_len + 1, "%.*s", event->topic_len, event->topic);
      ESP_LOGI(TAG, "ACK received on %s: %s", topic, data);
      cJSON *json = cJSON_Parse(data);
      if (!json) break;

      if (json_get_string(json, "status", &status_rx)) {
        if (strncmp(status_rx, "valid", 5) == 0) {
          got_ack = true;
        }
      }
      got_err_ack = true;
      cJSON_Delete(json);
      break;
    }
    default:
      break;
  }
}

esp_err_t mqtt_verify_client_run(const char *broker_uri, const char *verify_payload) {
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
    reg_client, ESP_EVENT_ANY_ID, mqtt_event_handler, (void *)verify_payload
  );
  esp_mqtt_client_start(reg_client);

  // Wait until ack received or timeout (15s)
  int retry = 0;
  int retry_count = 15;
  while ((!got_ack || !got_err_ack) && retry++ < retry_count) {
    ESP_LOGI(TAG, "Waiting for verify ack... (%2d/%d)", retry, retry_count);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  esp_mqtt_client_stop(reg_client);
  esp_mqtt_client_destroy(reg_client);

  if (got_ack)
    ESP_LOGI(TAG, "Verification complete: valid credentials");
  else {
    ESP_LOGW(TAG, "Verification failed: timeout or invalid");
  }

  return got_ack && !got_err_ack ? ESP_OK : ESP_FAIL;
}
