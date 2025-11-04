#ifndef SN_INET_H
#define SN_INET_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t sn_inet_init(void *args);

esp_err_t sn_inet_wifi_connect(const char *ssid, const char *password);

int sn_inet_get_wifi_rssi(void);

bool sn_inet_is_connected();

#endif // !SN_WIFI_H
