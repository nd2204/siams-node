#include "sn_mqtt_manager.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "sn_json.h"
#include "sn_mqtt_router.h"
#include "sn_sntp.h"
#include "sn_storage.h"
#include "sn_telemetry_queue.h"
#include "sn_topic.h"
#include <string.h>
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"

static const char *TAG = "SN_MQTT_MANAGER";

static esp_mqtt_client_handle_t client = NULL;
static sn_mqtt_msg_cb_t msg_callback = NULL;
static void *msg_arg = NULL;
static QueueHandle_t s_mqtt_pubq = NULL;
static TaskHandle_t s_pub_task = NULL;

static EventGroupHandle_t s_mqtt_event_group;
#define MQTT_CONNECTED_BIT    BIT0
#define MQTT_DISCONNECTED_BIT BIT1

typedef enum { MSG_TELEMETRY, MSG_STATUS, MSG_EVENT, MSG_COMMAND_ACK } mqtt_msg_type_e;

typedef struct {
  char topic[256];
  char payload[256];
  mqtt_msg_type_e type;
  int qos;
  bool retain;
} mqtt_publish_msg_t;

// ---------- Internal: MQTT publisher task ----------
static void pub_task(void *arg) {
  mqtt_publish_msg_t msg;

  while (1) {
    if (xQueueReceive(s_mqtt_pubq, &msg, portMAX_DELAY) == pdTRUE) {
      int msg_id = esp_mqtt_client_publish(client, msg.topic, msg.payload, 0, 1, 0);
      if (msg_id >= 0) {
        ESP_LOGI(TAG, "Tx [%d]: %s ", msg_id, msg.payload);
      } else {
        ESP_LOGE(TAG, "Failed to publish topic %s", msg.topic);
      }
    }
  }
}

// ---------- Internal: MQTT event handler ----------
static void mqtt_event_handler(
  void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data
) {
  esp_mqtt_event_handle_t event = event_data;

  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
      break;

    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGW(TAG, "Disconnected from MQTT broker");
      xEventGroupSetBits(s_mqtt_event_group, MQTT_DISCONNECTED_BIT);
      break;

    case MQTT_EVENT_DATA: {
      char topic[MAX_TOPIC_LEN], payload[1024];
      memset(topic, 0, sizeof(topic));
      memset(payload, 0, sizeof(payload));
      strncpy(topic, event->topic, event->topic_len);
      strncpy(payload, event->data, event->data_len);

      ESP_LOGI(TAG, "RX [%s]:\n%s", topic, payload);

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

  // WARN: ensure each string got sufficient len so they don't overflow
  // to the next buffer
  char mqtt_user[64] = {0};
  char mqtt_pass[64] = {0};
  char lwt_payload[64] = {0};
  char broker_uri[128] = {0};
  char lwt_topic[MAX_TOPIC_LEN] = {0};

  s_mqtt_pubq = xQueueCreate(16, sizeof(mqtt_publish_msg_t));
  if (!s_mqtt_pubq) return ESP_ERR_NO_MEM;
  s_mqtt_event_group = xEventGroupCreate();

  // Read credentials from NVS
  sn_storage_get_credentials(mqtt_user, sizeof(mqtt_user), mqtt_pass, sizeof(mqtt_pass));

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
    .session.last_will.qos = 1,
    .session.keepalive = 10
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
    return err;
  }

  ESP_LOGI(TAG, "Waiting for mqtt connection...");
  EventBits_t bits = xEventGroupWaitBits(
    s_mqtt_event_group, MQTT_CONNECTED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000)
  );

  if (bits & MQTT_CONNECTED_BIT) {
    ESP_LOGI(TAG, "Connected to MQTT broker");
    xTaskCreatePinnedToCore(pub_task, "publisher_task", 4096, NULL, 6, &s_pub_task, 0);
    sn_mqtt_router_init();
    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "MQTT connection failed or timeout");
    return ESP_FAIL;
  }

  return err;
}

// Enqueue message safely
static esp_err_t publisher_enqueue(mqtt_publish_msg_t *msg) {
  if (!s_mqtt_pubq) return ESP_ERR_INVALID_STATE;
  if (xQueueSend(s_mqtt_pubq, msg, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGW(TAG, "Publish queue full, message dropped");
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t sn_mqtt_publish_enqueue(const char *topic, const char *payload, int qos, bool retain) {
  mqtt_publish_msg_t msg = {.type = MSG_EVENT, .qos = qos, .retain = retain};
  strncpy(msg.topic, topic, sizeof(msg.topic));
  strncpy(msg.payload, payload, sizeof(msg.payload) - 1);
  return publisher_enqueue(&msg);
}

esp_err_t publish_telemetry(const sn_sensor_reading_t *reading) {
  if (!reading) return ESP_ERR_INVALID_ARG;

  mqtt_publish_msg_t msg = {.type = MSG_TELEMETRY, .qos = 0, .retain = false};
  const char *topic = sn_mqtt_topic_cache_get()->telemetry_topic;
  strncpy(msg.topic, topic, sizeof(msg.topic));
  cJSON *json = sensor_reading_to_json_obj(reading);
  if (!json) return ESP_ERR_NO_MEM;
  char *json_str = cJSON_PrintUnformatted(json);
  strncpy(msg.payload, json_str, sizeof(msg.payload) - 1);
  cJSON_free(json_str);
  cJSON_Delete(json);
  return publisher_enqueue(&msg);
}

esp_err_t publish_status(const sn_status_reading_t *status) {
  if (!status) return ESP_ERR_INVALID_ARG;
  mqtt_publish_msg_t msg = {.type = MSG_STATUS, .qos = 0, .retain = false};
  const char *topic = sn_mqtt_topic_cache_get()->status_topic;

  cJSON *json = cJSON_CreateObject();
  cJSON_AddNumberToObject(json, "cpu", status->cpu);
  cJSON_AddNumberToObject(json, "mem", status->mem);
  cJSON_AddNumberToObject(json, "wifi", status->wifi);
  cJSON_AddBoolToObject(json, "online", true);
  cJSON_AddNumberToObject(json, "ts", status->ts);
  char *json_str = cJSON_PrintUnformatted(json);

  strncpy(msg.topic, topic, sizeof(msg.topic));
  strncpy(msg.payload, json_str, sizeof(msg.payload) - 1);

  cJSON_free(json_str);
  cJSON_Delete(json);

  return publisher_enqueue(&msg);
}

esp_err_t sn_mqtt_subscribe(const char *topic, int qos) {
  if (!client) {
    ESP_LOGW(TAG, "Cannot subscribe, Client not connected");
    return ESP_FAIL;
  }

  int msg_id = esp_mqtt_client_subscribe(client, topic, qos);
  return msg_id >= 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t sn_mqtt_register_handler(sn_mqtt_msg_cb_t cb, void *arg) {
  msg_callback = cb;
  msg_arg = arg;
  ESP_LOGI(TAG, "MQTT message callback registered");
  return ESP_OK;
}

esp_err_t sn_mqtt_stop() { return esp_mqtt_client_stop(client); }

esp_err_t sn_mqtt_destroy() { return esp_mqtt_client_destroy(client); }

cJSON *parse_rx_payload(const void *event) {
  if (!event) return NULL;
  esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)event;
  char *buf = malloc(e->data_len + 1);
  memcpy(buf, e->data, e->data_len);
  buf[e->data_len] = '\0';
  return cJSON_Parse(buf);
  free(buf);
}

void mqtt_debug_print_rx(const void *event) {
  esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)event;
  char topic[MAX_TOPIC_LEN];
  snprintf(topic, sizeof(topic), "%.*s", e->topic_len, e->topic);
  char data[1024];
  snprintf(data, sizeof(data), "%.*s", e->data_len, e->data);
  ESP_LOGI(TAG, "RX [%s]: %s", topic, data);
}
