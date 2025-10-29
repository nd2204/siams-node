#ifndef SN_MQTT_ROUTER_H
#define SN_MQTT_ROUTER_H

#include "esp_err.h"
#include <stdbool.h>

// Callback type cho handler
typedef void (*sn_mqtt_message_handler_t)(const char *topic, const char *payload);

typedef struct {
  const char *org_id;
  const char *cluster_id;
  const char *device_id;
} sn_mqtt_context_t;

esp_err_t sn_mqtt_router_init(const sn_mqtt_context_t *ctx);

// Device -> Backend (publishers)
esp_err_t sn_mqtt_router_publish_telemetry(const char *sensor_key, const char *payload);
esp_err_t sn_mqtt_router_publish_status(const char *status_json);
esp_err_t sn_mqtt_router_publish_event(const char *event_json);

// for ack or command messages
esp_err_t sn_mqtt_router_subscriber_add(
  const char *topic, sn_mqtt_message_handler_t handler, int qos
);

#endif // SN_MQTT_ROUTER_H
