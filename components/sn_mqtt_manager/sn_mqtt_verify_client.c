#include "sn_json.h"
#include "sn_storage.h"
#include "sn_topic.h"
#include "sn_mqtt_verify_client.h"

#include "cJSON.h"
#include "mqtt_client.h"
#include "esp_log.h"

#define MQTT_CONNECTED_BIT BIT0
#define MQTT_ACK_RECEIVED  BIT1
#define MQTT_ACK_ERROR     BIT2

static const char *TAG = "SN_MQTT_VERIFY_CLIENT";
static esp_mqtt_client_handle_t verify_client = NULL;
static EventGroupHandle_t mqtt_evt_group;

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
      xEventGroupSetBits(mqtt_evt_group, MQTT_EVENT_CONNECTED);
      // Subscribe to ack topic
      const sn_mqtt_topic_cache_t *cache = sn_mqtt_topic_cache_get();
      // Subscribe to verify ack
      esp_mqtt_client_subscribe(verify_client, cache->verify_ack_topic, 1);
      // Publish to verify
      esp_mqtt_client_publish(verify_client, cache->verify_topic, payload, 0, 1, false);
      break;
    }

    case MQTT_EVENT_DATA: {
      cJSON *json = cJSON_Parse(event->data);
      if (!json) break;
      char *payload_str = cJSON_PrintUnformatted(json);
      if (payload_str) {
        ESP_LOGI(TAG, "Rx: %s", payload_str);
        cJSON_free(payload_str);
      }
      if (json_get_string(json, "status", &status_rx)) {
        if (strncmp(status_rx, "ok", 2) == 0) {
          xEventGroupSetBits(mqtt_evt_group, MQTT_ACK_RECEIVED);
        } else {
          xEventGroupSetBits(mqtt_evt_group, MQTT_ACK_ERROR);
        }
      } else {
        xEventGroupSetBits(mqtt_evt_group, MQTT_ACK_ERROR);
      }
      cJSON_Delete(json);
      break;
    }
    default:
      break;
  }
}

esp_err_t mqtt_verify_client_run(const char *broker_uri, const char *verify_payload) {
  char mqtt_user[64] = {0};
  char mqtt_pass[64] = {0};

  sn_storage_get_credentials(mqtt_user, sizeof(mqtt_user), mqtt_pass, sizeof(mqtt_pass));

  esp_mqtt_client_config_t cfg = {
    .broker.address.uri = broker_uri,
    .credentials.username = mqtt_user[0] ? mqtt_user : NULL,
    .credentials.authentication.password = mqtt_pass[0] ? mqtt_user : NULL
  };

  verify_client = esp_mqtt_client_init(&cfg);
  esp_mqtt_client_register_event(
    verify_client, ESP_EVENT_ANY_ID, mqtt_event_handler, (void *)verify_payload
  );

  mqtt_evt_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_mqtt_client_start(verify_client));
  ESP_LOGI(TAG, "Waiting for verify ack... ");

  EventBits_t connected_bit =
    xEventGroupWaitBits(mqtt_evt_group, MQTT_CONNECTED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(30000));
  if (connected_bit & MQTT_CONNECTED_BIT) {
    ESP_LOGI(TAG, "Connected to broker (verification mode)");
    // Wait until ack received or timeout (30s)
    EventBits_t bits = xEventGroupWaitBits(
      mqtt_evt_group, MQTT_ACK_RECEIVED | MQTT_ACK_ERROR,
      pdTRUE,  // clear bits on exit
      pdFALSE, // wait for any bit
      pdMS_TO_TICKS(30000)
    );

    esp_mqtt_client_stop(verify_client);
    esp_mqtt_client_destroy(verify_client);
    verify_client = NULL;

    if (bits & MQTT_ACK_RECEIVED) {
      ESP_LOGI(TAG, "Ack received!");
      return ESP_OK;
    } else if (bits & MQTT_ACK_ERROR) {
      ESP_LOGE(TAG, "Error ack received!");
      return ESP_FAIL;
    } else {
      ESP_LOGE(TAG, "Timeout waiting for ack");
      return ESP_FAIL;
    }
  }

  return ESP_FAIL;
}
