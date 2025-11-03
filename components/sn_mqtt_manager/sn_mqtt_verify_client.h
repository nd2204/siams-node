#ifndef SN_MQTT_VERIFY_CLIENT_H
#define SN_MQTT_VERIFY_CLIENT_H

#include "esp_err.h"

esp_err_t mqtt_verify_client_run(const char *broker_uri, const char *verify_payload);

#endif // !SN_MQTT_REGISTRATION_CLIENT_H
