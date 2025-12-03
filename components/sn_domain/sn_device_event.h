#ifndef SN_DEVICE_EVENT_H
#define SN_DEVICE_EVENT_H

#include "cJSON.h"

typedef enum {
  DEVICE_EVENT_RULE_TRIGGERED,
  DEVICE_EVENT_RULE_CREATED,
} sn_device_event_e;

typedef struct {
  sn_device_event_e event_type;
} sn_device_event_t;

static inline cJSON *sn_device_event_to_json(const sn_device_event_t *event) {
  cJSON *ret = cJSON_CreateObject();
  return ret;
}

#endif // !SN_DEVICE_EVENT_H
