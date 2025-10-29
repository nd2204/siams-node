
#include "cJSON.h"
#include <stdbool.h>
#include <stdint.h>
#include "sn_handlers.h"

DECLARE_ACTION_HANDLER(pump_handler) {
  const cJSON *p_enable = cJSON_GetObjectItemCaseSensitive(params, "enable");
  bool enable = false;

  if (cJSON_IsBool(p_enable))
    enable = cJSON_IsTrue(p_enable);
  else if (cJSON_IsNumber(p_enable))
    enable = (p_enable->valueint != 0);
  // implement hardware control accordingly...

  if (out_result) {
    *out_result = cJSON_CreateObject();
    cJSON_AddStringToObject(*out_result, "actuator", "pump");
    cJSON_AddBoolToObject(*out_result, "state", enable);
  }
  return 0;
}
