// --------------------------------------------------------------------------------
// sn_mqtt_ephemeral_client.h
//
// description: run a client instance and wait for the subscribed topic once
// author: nd2204
// --------------------------------------------------------------------------------

#ifndef SN_MQTT_EPHEMERAL_CLIENT_H
#define SN_MQTT_EPHEMERAL_CLIENT_H

#include "cJSON.h"
#include "esp_err.h"

typedef void (*on_mqtt_message)(cJSON *message);

typedef struct {
  const char *pub_topic;
  const char *sub_topic;
  const char *pub_payload;
  on_mqtt_message on_message;
  uint32_t timeout_ms;
} sn_mqtt_ephemeral_client_opts;

esp_err_t sn_mqtt_ephemeral_client_run_and_wait(const sn_mqtt_ephemeral_client_opts *opts);

#endif // !SN_MQTT_EPHEMERAL_CLIENT_H
