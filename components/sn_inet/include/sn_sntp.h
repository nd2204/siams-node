#ifndef SN_SNTP_H
#define SN_SNTP_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <sys/time.h>
#include <time.h>

esp_err_t sn_init_sntp(void);

bool sn_is_time_synced(time_t timestamp);

void sn_get_iso8601_timestamp(char *buffer, size_t buffer_size);

void sn_timestamp_to_iso8601(time_t timestamp, char *buffer, size_t buffer_size);

#endif // !SN_SNTP_H
