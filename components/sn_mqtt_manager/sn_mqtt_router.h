#ifndef SN_MQTT_ROUTER_H
#define SN_MQTT_ROUTER_H

#include "esp_err.h"
#include <stdbool.h>

// Callback type cho handler
typedef void (*sn_mqtt_message_handler_t)(const char *topic, const char *payload);

esp_err_t sn_mqtt_router_init();

// for ack or command messages
esp_err_t sn_mqtt_router_subscriber_add(
  const char *topic, sn_mqtt_message_handler_t handler, int qos
);

#endif // SN_MQTT_ROUTER_H
