#ifndef SN_MQTT_REGISTRATION_CLIENT_H
#define SN_MQTT_REGISTRATION_CLIENT_H

#include "esp_err.h"

void gen_temp_id(char *out, size_t len);

esp_err_t mqtt_registration_run(const char *broker_uri, const char *registerPayload);

#endif // !SN_MQTT_REGISTRATION_CLIENT_H
