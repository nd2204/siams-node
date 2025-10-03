#ifndef SN_WIFI_H
#define SN_WIFI_H

#include "sn_error.h"

esp_err_t sn_wifi_init();

esp_err_t sn_wifi_connect(const char *ssid, const char *password);

#endif // !SN_WIFI_H
