#include "cJSON.h"
#include "esp_log.h"
#include "sn_capability.h"
#include "sn_json.h"
#include "sn_mqtt_ephemeral_client.h"
#include "sn_security.h"
#include "sn_storage.h"
#include "sn_topic.h"

static bool success = false;
static const char *TAG = "recover_device";

static cJSON *create_recover_payload() {
  char hw_id[64];
  get_hwid(hw_id, sizeof(hw_id));
  // create raw payload string
  cJSON *payload = cJSON_CreateObject();
  cJSON_AddStringToObject(payload, "hw_id", hw_id);
  // clean old resource
  return sn_security_sign_and_wrap_payload(payload);
}

static void on_message(cJSON *message) {
  const char *device_id;

  success = (json_get_string(message, "device_id", &device_id));

  if (success) {
    ESP_LOGI(TAG, "got device_id=%s", device_id);
    const sn_mqtt_topic_context_t *ctx = sn_mqtt_topic_cache_get_context();
    sn_mqtt_topic_context_t new_ctx = *ctx;
    strncpy(new_ctx.deviceId, device_id, sizeof(new_ctx.deviceId));
    sn_mqtt_topic_cache_set_context(&new_ctx);
  } else {
    sn_storage_erase_device_secret();
  }
}

esp_err_t recover_device() {
  char recover_topic_pub[MAX_TOPIC_LEN] = {0};
  char recover_topic_sub[MAX_TOPIC_LEN] = {0};
  sn_build_device_topic_from_ctx(
    recover_topic_pub, sizeof(recover_topic_pub), TOPIC_REGISTER_ACK_FMT
  );
  sn_build_device_topic_from_ctx(
    recover_topic_sub, sizeof(recover_topic_sub), TOPIC_RECOVER_ACK_FMT
  );

  cJSON *payload_json = create_recover_payload();
  char *payload_str = cJSON_PrintUnformatted(payload_json);
  assert(payload_json != NULL);

  sn_mqtt_ephemeral_client_opts opts = {0};
  opts.on_message = on_message;
  opts.timeout_ms = 30000;
  opts.sub_topic = recover_topic_sub;
  opts.pub_topic = recover_topic_pub;
  opts.pub_payload = payload_str;

  ESP_LOGI(TAG, "running recovery from device_secret...");
  esp_err_t err = sn_mqtt_ephemeral_client_run_and_wait(&opts);
  if (success) {
    ESP_LOGE(TAG, "Recover success");
  } else if (err != ESP_OK) {
    sn_storage_erase_device_id();
    ESP_LOGE(TAG, "Recover failed");
  } else if (err == ESP_ERR_TIMEOUT) {
    ESP_LOGE(TAG, "Registration timed out");
  }

  err = success ? ESP_OK : ESP_FAIL;

  cJSON_free(payload_str);
  cJSON_Delete(payload_json);
  return err;
}
