#include "sn_mqtt_ephemeral_client.h"
#include "sdkconfig.h"
#include "sn_json.h"
#include "sn_storage.h"
#include "sn_topic.h"

#include "cJSON.h"
#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "SN_MQTT_EPHEMERAL_CLIENT";
static esp_mqtt_client_handle_t mqtt_client = NULL;
static EventGroupHandle_t mqtt_evt_group;

#if !defined(CONFIG_MQTT_BROKER_URI)
#error "CONFIG_MQTT_BROKER_URI is required"
#endif

#define MQTT_CONNECTED_BIT BIT(0)
#define MQTT_MESSAGE_BIT   BIT(1)

static void mqtt_event_handler(void *args, esp_event_base_t base, int32_t eid, void *edata) {
  esp_mqtt_event_handle_t event = edata;
  const sn_mqtt_ephemeral_client_opts *opts = (const sn_mqtt_ephemeral_client_opts *)args;

  switch ((esp_mqtt_event_id_t)eid) {
    case MQTT_EVENT_CONNECTED: {
      xEventGroupSetBits(mqtt_evt_group, MQTT_EVENT_CONNECTED);
      break;
    }

    case MQTT_EVENT_DATA: {
      cJSON *json = cJSON_Parse(event->data);
      if (!json) {
        ESP_LOGE(TAG, "Rx: Malformed json object");
        xEventGroupSetBits(mqtt_evt_group, MQTT_MESSAGE_BIT);
        break;
      };
      char *payload_str = cJSON_PrintUnformatted(json);
      if (payload_str) {
        ESP_LOGI(TAG, "Rx: %s", payload_str);
        cJSON_free(payload_str);
      }
      opts->on_message(json);
      xEventGroupSetBits(mqtt_evt_group, MQTT_MESSAGE_BIT);
      break;
    }
    default:
      break;
  }
}

esp_err_t sn_mqtt_ephemeral_client_run_and_wait(const sn_mqtt_ephemeral_client_opts *opts) {
  if (!opts->pub_topic || !opts->sub_topic || !opts->pub_payload) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!opts->on_message) {
    ESP_LOGW(TAG, "opt.on_message is NULL");
  }

  char mqtt_user[64] = {0};
  char mqtt_pass[64] = {0};

  sn_storage_get_credentials(mqtt_user, sizeof(mqtt_user), mqtt_pass, sizeof(mqtt_pass));

  esp_mqtt_client_config_t cfg = {
    .broker.address.uri = CONFIG_MQTT_BROKER_URI,
    .credentials.username = mqtt_user[0] ? mqtt_user : NULL,
    .credentials.authentication.password = mqtt_pass[0] ? mqtt_user : NULL
  };

  mqtt_client = esp_mqtt_client_init(&cfg);
  esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, (void *)opts);
  mqtt_evt_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));

  EventBits_t connected_bit =
    xEventGroupWaitBits(mqtt_evt_group, MQTT_CONNECTED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(30000));

  if (connected_bit & MQTT_CONNECTED_BIT) {
    esp_mqtt_client_subscribe(mqtt_client, opts->sub_topic, 1);
    esp_mqtt_client_publish(mqtt_client, opts->pub_topic, opts->pub_payload, 0, 1, false);

    // Wait until ack received or timeout (30s)
    EventBits_t bits = xEventGroupWaitBits(
      mqtt_evt_group, MQTT_MESSAGE_BIT,
      pdTRUE,  // clear bits on exit
      pdFALSE, // wait for any bit
      pdMS_TO_TICKS(opts->timeout_ms)
    );

    if (mqtt_client) {
      esp_mqtt_client_stop(mqtt_client);
      esp_mqtt_client_destroy(mqtt_client);
      mqtt_client = NULL;
    }

    return bits & MQTT_MESSAGE_BIT ? ESP_OK : ESP_ERR_TIMEOUT;
  } else {
    return ESP_FAIL;
  }
}
