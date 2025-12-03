#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sn_json.h"
#include "sn_mqtt_ephemeral_client.h"
#include "sn_security.h"
#include "sn_storage.h"
#include "sn_topic.h"

static bool success = false;
static const char *TAG = "verify_device";

static void on_message(cJSON *message) {
  char *str = cJSON_Print(message);
  if (str) {
    printf("verify-ack: %s", str);
  }

  bool ok = false;
  if (json_get_bool(message, "ok", &ok)) {
    if (ok) {
      success = true;
    } else {
      ESP_LOGE(TAG, "Erasing device_id from storage");
      sn_storage_erase_device_id();
      // WARN: this only here for debugging
      sn_storage_erase_device_secret();
    }
  }
}

esp_err_t verify_device() {
  char verify_topic[MAX_TOPIC_LEN] = {0};
  char verify_topic_ack[MAX_TOPIC_LEN] = {0};
  sn_build_device_topic_from_ctx(verify_topic, sizeof(verify_topic), TOPIC_DEV_VERIFY_FMT);
  sn_build_device_topic_from_ctx(
    verify_topic_ack, sizeof(verify_topic_ack), TOPIC_DEV_VERIFY_ACK_FMT
  );
  cJSON *payload_json = sn_security_sign_and_wrap_payload(cJSON_Parse("{\"fw_ver\":\"v1.0.0\"}"));
  char *payload_str = cJSON_PrintUnformatted(payload_json);
  assert(payload_json != NULL);

  sn_mqtt_ephemeral_client_opts opts = {0};
  opts.on_message = on_message;
  opts.timeout_ms = 30000;
  opts.sub_topic = verify_topic_ack;
  opts.pub_topic = verify_topic;
  opts.pub_payload = payload_str;

  ESP_LOGI(TAG, "running verification using device_id, device_secret...");
  // sent verify if deviceId exists
  esp_err_t err = sn_mqtt_ephemeral_client_run_and_wait(&opts);
  if (success) {
    ESP_LOGI(TAG, "Verification success");
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG, "Verification failed");
  } else if (err == ESP_ERR_TIMEOUT) {
    ESP_LOGE(TAG, "Verification timed out");
  }

  err = success ? ESP_OK : ESP_FAIL;

  cJSON_free(payload_str);
  cJSON_Delete(payload_json);
  return err;
}
