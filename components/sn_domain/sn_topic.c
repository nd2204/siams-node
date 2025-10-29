#include "sn_topic.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

void sn_init_topics() {}

static const char *TAG = "mqtt_topic_cache";
static sn_mqtt_topic_context_t ctx;
static sn_mqtt_topic_cache_t cache;

#define GEN_BUILD_TOPIC_FN(name, fmt)                                                              \
  static inline void build_##name##_topic(                                                         \
    char *buf, size_t len, const char *org, const char *cluster, const char *device                \
  ) {                                                                                              \
    snprintf(buf, len, fmt, org, cluster, device);                                                 \
  }
TOPIC_DEVICE_CONFIG(GEN_BUILD_TOPIC_FN)
#undef GEN_BUILD_TOPIC_FN

// Internal helper
static void rebuild_topics(void) {
#define gen_fill_cache(name, fmt)                                                                  \
  build_##name##_topic(                                                                            \
    cache.name##_topic, sizeof(cache.name##_topic), ctx.orgId, ctx.clusterId, ctx.deviceId         \
  );
  TOPIC_DEVICE_CONFIG(gen_fill_cache)
#undef gen_fill_cache
  ESP_LOGI(TAG, "MQTT topics rebuilt for %s/%s/%s", ctx.orgId, ctx.clusterId, ctx.deviceId);
}

void print_topic_cache(void) {
#define gen_print_cache(name, fmt) ESP_LOGI(TAG, "%s", cache.name##_topic);
  TOPIC_DEVICE_CONFIG(gen_print_cache)
#undef gen_print_cache
}

//--------------------------------------------------------------------------------
// API
//--------------------------------------------------------------------------------

esp_err_t sn_mqtt_topic_cache_init(const sn_mqtt_topic_context_t *context) {
  return sn_mqtt_topic_cache_set_context(context);
}

esp_err_t sn_mqtt_topic_cache_set_context(const sn_mqtt_topic_context_t *context) {
  if (!context) return ESP_ERR_INVALID_ARG;
  memcpy(&ctx, context, sizeof(sn_mqtt_topic_context_t));
  memset(&cache, 0, sizeof(sn_mqtt_topic_cache_t));
  rebuild_topics();
  return ESP_OK;
}

const sn_mqtt_topic_context_t *sn_mqtt_topic_cache_get_context(void) { return &ctx; }

const sn_mqtt_topic_cache_t *sn_mqtt_topic_cache_get(void) { return &cache; }

//--------------------------------------------------------------------------------
// payload builders
//--------------------------------------------------------------------------------

cJSON *create_lwt_payload_json(const char *isots) {
  cJSON *root = cJSON_CreateObject();
  if (!root) return NULL;
  cJSON_AddStringToObject(root, "ts", isots);
  cJSON_AddBoolToObject(root, "online", 0);
  return root;
}

cJSON *create_telemetry_payload_json(local_id_t localId, double value, const char *isots) {
  cJSON *root = cJSON_CreateObject();
  if (!root) return NULL;
  cJSON_AddNumberToObject(root, "localId", localId);
  cJSON_AddNumberToObject(root, "value", value);
  cJSON_AddStringToObject(root, "ts", isots);
  return root;
}
