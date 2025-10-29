#ifndef SN_CAPABILITY_H
#define SN_CAPABILITY_H

#include "cJSON.h"
#include "sn_driver/port_desc.h"
#include <stdint.h>

#define LOCAL_ID_MAX 0xff
#define LOCAL_ID_MIN 0x01

#define DECLARE_ACTION_HANDLER(FNNAME) int FNNAME(const cJSON *params, cJSON **out_result)

typedef int (*action_handler_t)(const cJSON *params, cJSON **out_result);

typedef struct {
  const char *action;
  const char **params;
  action_handler_t handler;
} command_capability_t;

cJSON *device_ports_to_capabilities_json();

int dispatch_command(const char *payload_json, cJSON **out_result);

#endif // !SN_CAPABILITY_H
