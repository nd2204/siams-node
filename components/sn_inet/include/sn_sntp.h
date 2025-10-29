#ifndef SN_SNTP_H
#define SN_SNTP_H

#include "esp_err.h"
#include <stddef.h>

esp_err_t sn_init_sntp(void);
void sn_get_iso8601_timestamp(char *buffer, size_t buffer_size);

#endif // !SN_SNTP_H
