#include "freertos/idf_additions.h"
#include "sn_common.h"
#include "sn_json.h"
#include "sn_mqtt_manager.h"
#include "sn_storage.h"
#include "sn_topic.h"
#include "sn_mqtt_registration_client.h"

#include "cJSON.h"
#include "esp_random.h"
#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "SN_MQTT_REG_CLIENT";
static esp_mqtt_client_handle_t reg_client = NULL;
static EventGroupHandle_t mqtt_evt_group;

#define MQTT_CONNECTED_BIT BIT0
#define MQTT_ACK_RECEIVED  BIT1
#define MQTT_ACK_ERROR     BIT2

void gen_temp_id(char *out, size_t len) {
  uint32_t r = esp_random();
  snprintf(out, len, "temp-%08lx", r);
}

// Callback for MQTT events
static void mqtt_event_handler(void *args, esp_event_base_t base, int32_t eid, void *edata) {
  esp_mqtt_event_handle_t event = edata;
  const char *payload = (const char *)args;
  const char *device_id_rx = NULL;

  if (!payload) {
    ESP_LOGE(TAG, "payload is null or empty");
    return;
  }

  switch ((esp_mqtt_event_id_t)eid) {
    case MQTT_EVENT_CONNECTED: {
      xEventGroupSetBits(mqtt_evt_group, MQTT_EVENT_CONNECTED);
      // Subscribe to ack topic
      const sn_mqtt_topic_context_t *ctx = sn_mqtt_topic_cache_get_context();
      char ack_topic[MAX_TOPIC_LEN];
      snprintf(
        ack_topic, sizeof(ack_topic), TOPIC_REGISTER_ACK_FMT, ctx->orgId, ctx->clusterId,
        ctx->deviceId
      );
      esp_mqtt_client_subscribe(reg_client, ack_topic, 1);

      // Publish to register topic
      char reg_topic[MAX_TOPIC_LEN];
      snprintf(
        reg_topic, sizeof(reg_topic), TOPIC_REGISTER_FMT, ctx->orgId, ctx->clusterId, ctx->deviceId
      );
      esp_mqtt_client_publish(reg_client, reg_topic, payload, 0, 1, false);
      break;
    }
    case MQTT_EVENT_DATA: {
      cJSON *json = parse_rx_payload(event);
      if (!json) break;
      // char *payload_str = cJSON_PrintUnformatted(json);
      // if (payload_str) {
      //   ESP_LOGI(TAG, "RX: %s", payload_str);
      //   cJSON_free(payload_str);
      // }

      if (json_get_string(json, "deviceId", &device_id_rx)) {
        ESP_LOGI(TAG, "Device registered successfully: %s", device_id_rx);
        sn_storage_set_device_id(device_id_rx);
        xEventGroupSetBits(mqtt_evt_group, MQTT_ACK_RECEIVED);
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

esp_err_t mqtt_registration_run(const char *broker_uri, const char *register_payload) {
  char mqtt_user[64] = {0};
  char mqtt_pass[64] = {0};

  sn_storage_get_credentials(mqtt_user, sizeof(mqtt_user), mqtt_pass, sizeof(mqtt_pass));

  esp_mqtt_client_config_t cfg = {
    .broker.address.uri = broker_uri,
    .credentials.username = mqtt_user[0] ? mqtt_user : NULL,
    .credentials.authentication.password = mqtt_pass[0] ? mqtt_pass : NULL
  };
  reg_client = esp_mqtt_client_init(&cfg);
  esp_mqtt_client_register_event(
    reg_client, ESP_EVENT_ANY_ID, mqtt_event_handler, (void *)register_payload
  );

  mqtt_evt_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_mqtt_client_start(reg_client));

  EventBits_t connected_bit =
    xEventGroupWaitBits(mqtt_evt_group, MQTT_CONNECTED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(30000));
  if (connected_bit & MQTT_EVENT_CONNECTED) {
    ESP_LOGI(TAG, "Connected to broker (registration mode)");
    // Wait until ack received or timeout (30s)
    ESP_LOGI(TAG, "Waiting for register ack...");
    EventBits_t bits = xEventGroupWaitBits(
      mqtt_evt_group, MQTT_ACK_RECEIVED | MQTT_ACK_ERROR,
      pdTRUE,  // clear bits on exit
      pdFALSE, // wait for any bit
      pdMS_TO_TICKS(30000)
    );

    esp_mqtt_client_stop(reg_client);
    esp_mqtt_client_destroy(reg_client);
    vEventGroupDelete(mqtt_evt_group);

    reg_client = NULL;
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
