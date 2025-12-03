#include "cJSON.h"
#include "esp_log.h"
#include "sn_capability.h"
#include "sn_json.h"
#include "sn_mqtt_ephemeral_client.h"
#include "sn_security.h"
#include "sn_storage.h"
#include "sn_topic.h"

static bool success = false;
static const char *TAG = "register_device";

static cJSON *create_register_payload() {
  cJSON *capabilities_json = device_ports_to_capabilities_json();
  if (capabilities_json) {
    return sn_security_sign_and_wrap_payload(capabilities_json);
  }
  return NULL;
}

static void on_message(cJSON *message) {
  const char *device_id;
  const char *device_secret;

  success = (json_get_string(message, "deviceId", &device_id));
  success = (json_get_string(message, "device_secret", &device_secret));

  if (success) {
    ESP_LOGI(TAG, "got device_id=%s", device_id);
    ESP_LOGI(TAG, "got device_secret=%s", device_secret);

    // Configure new topic context for subsequence action
    const sn_mqtt_topic_context_t *ctx = sn_mqtt_topic_cache_get_context();
    sn_mqtt_topic_context_t new_ctx = *ctx;
    strncpy(new_ctx.deviceId, device_id, sizeof(new_ctx.deviceId));
    sn_mqtt_topic_cache_set_context(&new_ctx);

    // Store device id and secret
    sn_storage_set_device_id(device_id);
    sn_storage_set_device_secret(device_secret);
  }
}

esp_err_t register_device() {
  char register_topic_pub[MAX_TOPIC_LEN] = {0};
  char register_topic_sub[MAX_TOPIC_LEN] = {0};
  sn_build_device_topic_from_ctx(
    register_topic_pub, sizeof(register_topic_pub), TOPIC_REGISTER_FMT
  );
  sn_build_device_topic_from_ctx(
    register_topic_sub, sizeof(register_topic_sub), TOPIC_REGISTER_ACK_FMT
  );

  cJSON *payload_json = create_register_payload();
  char *payload_str = cJSON_PrintUnformatted(payload_json);
  assert(payload_json != NULL);

  sn_mqtt_ephemeral_client_opts opts = {0};
  opts.on_message = on_message;
  opts.timeout_ms = 30000;
  opts.sub_topic = register_topic_sub;
  opts.pub_topic = register_topic_pub;
  opts.pub_payload = payload_str;

  ESP_LOGI(TAG, "running registration...");
  esp_err_t err = sn_mqtt_ephemeral_client_run_and_wait(&opts);
  if (success) {
    ESP_LOGI(TAG, "Registration success");
    err = success ? ESP_OK : ESP_FAIL;
  } else if (err != ESP_OK) {
    sn_storage_erase_device_id();
    ESP_LOGE(TAG, "Registration failed erasing device_id for re-register");
  } else if (err == ESP_ERR_TIMEOUT) {
    ESP_LOGE(TAG, "Registration timed out");
  }

  cJSON_free(payload_str);
  cJSON_Delete(payload_json);
  return err;
}
