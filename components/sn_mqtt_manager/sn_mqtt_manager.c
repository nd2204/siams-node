#include "sn_mqtt_manager.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "sn_storage.h"
#include "sn_topic.h"
#include <string.h>

static const char *TAG = "SN_MQTT_MANAGER";

static esp_mqtt_client_handle_t client = NULL;
static sn_mqtt_msg_cb_t msg_callback = NULL;
static void *msg_arg = NULL;

static char mqtt_user[64];
static char mqtt_pass[64];
static char lwt_topic[128];
static char lwt_payload[64];
static char broker_uri[128];

static bool connected = false;

// ---------- Internal: MQTT event handler ----------
static void mqtt_event_handler(
  void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data
) {
  esp_mqtt_event_handle_t event = event_data;

  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      ESP_LOGI(TAG, "Connected to MQTT broker");
      connected = true;
      // Optionally publish online retained message
      break;

    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGW(TAG, "Disconnected from MQTT broker");
      connected = false;
      break;

    case MQTT_EVENT_DATA: {
      char topic[MAX_TOPIC_LEN], payload[1024];
      memset(topic, 0, sizeof(topic));
      memset(payload, 0, sizeof(payload));
      strncpy(topic, event->topic, event->topic_len);
      strncpy(payload, event->data, event->data_len);

      ESP_LOGI(TAG, "RX [%s]: %s", topic, payload);

      if (msg_callback) msg_callback(topic, payload, msg_arg);
    } break;

    case MQTT_EVENT_ERROR:
      ESP_LOGE(TAG, "MQTT error occurred");
      break;

    default:
      break;
  }
}

// ---------- Public API ----------

esp_err_t sn_mqtt_init(const sn_mqtt_config_t *cfg) {
  if (!cfg || !cfg->uri) {
    ESP_LOGE(TAG, "Invalid MQTT config");
    return ESP_ERR_INVALID_ARG;
  }

  // Read credentials from NVS
  esp_err_t err =
    sn_storage_get_credentials(mqtt_user, sizeof(mqtt_user), mqtt_pass, sizeof(mqtt_pass));
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "No credentials found, using anonymous connection");
    mqtt_user[0] = '\0';
    mqtt_pass[0] = '\0';
  }

  strncpy(broker_uri, cfg->uri, sizeof(broker_uri));
  if (cfg->lwt_topic) strncpy(lwt_topic, cfg->lwt_topic, sizeof(lwt_topic));
  if (cfg->lwt_payload) strncpy(lwt_payload, cfg->lwt_payload, sizeof(lwt_payload));

  const esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = broker_uri,
    .credentials.username = mqtt_user[0] ? mqtt_user : NULL,
    .credentials.authentication.password = mqtt_pass[0] ? mqtt_pass : NULL,
    .session.last_will.topic = lwt_topic[0] ? lwt_topic : NULL,
    .session.last_will.msg = lwt_payload[0] ? lwt_payload : NULL,
    .session.last_will.retain = true,
    .session.last_will.qos = 1
  };

  client = esp_mqtt_client_init(&mqtt_cfg);
  if (!client) {
    ESP_LOGE(TAG, "Failed to init MQTT client");
    return ESP_FAIL;
  }

  ESP_ERROR_CHECK(
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL)
  );
  ESP_LOGI(TAG, "MQTT manager initialized");
  return ESP_OK;
}

esp_err_t sn_mqtt_start(void) {
  if (!client) {
    ESP_LOGE(TAG, "Client not initialized");
    return ESP_FAIL;
  }

  esp_err_t err = esp_mqtt_client_start(client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
  }
  return err;
}

esp_err_t sn_mqtt_publish(const char *topic, const char *payload, int qos, bool retain) {
  if (!client || !connected) {
    ESP_LOGW(TAG, "Not connected, skip publish");
    return ESP_FAIL;
  }

  int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, qos, retain);
  if (msg_id >= 0) {
    ESP_LOGI(TAG, "TX [%s] id=%d: %s", topic, msg_id, payload);
    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "Failed to publish");
    return ESP_FAIL;
  }
}

esp_err_t sn_mqtt_subscribe(const char *topic, int qos) {
  if (!client || !connected) {
    ESP_LOGW(TAG, "Not connected, cannot subscribe yet");
    return ESP_FAIL;
  }

  int msg_id = esp_mqtt_client_subscribe(client, topic, qos);
  if (msg_id >= 0) {
    ESP_LOGI(TAG, "Subscribed to %s", topic);
    return ESP_OK;
  }
  ESP_LOGE(TAG, "Subscribe failed for %s", topic);
  return ESP_FAIL;
}

esp_err_t sn_mqtt_register_handler(sn_mqtt_msg_cb_t cb, void *arg) {
  msg_callback = cb;
  msg_arg = arg;
  ESP_LOGI(TAG, "MQTT message callback registered");
  return ESP_OK;
}

esp_err_t sn_mqtt_stop() { return esp_mqtt_client_stop(client); }

esp_err_t sn_mqtt_destroy() { return esp_mqtt_client_destroy(client); }

bool sn_mqtt_is_connected(void) { return connected; }
