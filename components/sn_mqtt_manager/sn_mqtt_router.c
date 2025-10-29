#include "sn_mqtt_router.h"
#include "sn_mqtt_manager.h"
#include "esp_log.h"
#include "sn_topic.h"
#include <stdio.h>
#include <string.h>

#define MAX_TOPIC_HANDLERS 10

static sn_mqtt_context_t ctx;
static const char *TAG = "mqtt_router";

typedef struct {
  char topic[128];
  sn_mqtt_message_handler_t handler;
} topic_handler_entry_t;

static topic_handler_entry_t registry[MAX_TOPIC_HANDLERS];
static size_t registry_count = 0;

// Central MQTT callback
static void on_mqtt_message(const char *topic, const char *payload, void *args) {
  ESP_LOGI(TAG, "Incoming: %s -> %s", topic, payload);

  for (size_t i = 0; i < registry_count; i++) {
    if (strcmp(topic, registry[i].topic) == 0) {
      if (registry[i].handler) {
        registry[i].handler(topic, payload);
        return;
      }
    }
  }

  ESP_LOGW(TAG, "No handler found for topic: %s", topic);
}

esp_err_t sn_mqtt_router_init(const sn_mqtt_context_t *context) {
  if (!context) {
    return ESP_ERR_INVALID_ARG;
  }
  ctx = *context;
  registry_count = 0;
  sn_mqtt_register_handler(on_mqtt_message, NULL);

  ESP_LOGI(
    TAG, "MQTT Subscriber initialized for %s/%s/%s", context->org_id, context->cluster_id,
    context->device_id
  );
  return ESP_OK;
}

esp_err_t sn_mqtt_router_publish_telemetry(const char *sensor_key, const char *payload) {
  char topic[128];
  // snprintf(topic, sizeof(topic), TOPIC_BASE "device/%s/telemetry/%s", ctx.device_id, sensor_key);
  const sn_mqtt_topic_cache_t *cache = sn_mqtt_topic_cache_get();
  snprintf(topic, sizeof(topic), cache->telemetry_topic, ctx.device_id);
  ESP_LOGI(TAG, "Publish TELEMETRY -> %s", topic);
  return sn_mqtt_publish(topic, payload, 1, false);
}

esp_err_t sn_mqtt_router_publish_status(const char *status_json) {
  char topic[128];
  snprintf(
    topic, sizeof(topic), "org/%s/clusters/%s/device/%s/status", ctx.org_id, ctx.cluster_id,
    ctx.device_id
  );
  return sn_mqtt_publish(topic, status_json, 1, false);
}

esp_err_t sn_mqtt_router_publish_event(const char *event_json) {
  char topic[128];
  snprintf(
    topic, sizeof(topic), "org/%s/clusters/%s/device/%s/event", ctx.org_id, ctx.cluster_id,
    ctx.device_id
  );
  return sn_mqtt_publish(topic, event_json, 1, false);
}

esp_err_t sn_mqtt_router_subscriber_add(
  const char *topic, sn_mqtt_message_handler_t handler, int qos
) {
  if (registry_count >= MAX_TOPIC_HANDLERS) {
    ESP_LOGE(TAG, "Topic handler limit reached");
    return ESP_ERR_NO_MEM;
  }

  strncpy(registry[registry_count].topic, topic, sizeof(registry[registry_count].topic));
  registry[registry_count].handler = handler;

  sn_mqtt_subscribe(topic, qos);
  ESP_LOGI(TAG, "Subscribed and registered handler for %s", topic);

  registry_count++;
  return ESP_OK;
}
