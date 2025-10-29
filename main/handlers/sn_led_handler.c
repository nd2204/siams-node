#include "cJSON.h"
#include "driver/gpio.h"
#include "sn_json.h"
#include "sn_handlers.h"
#include <stdbool.h>
#include <stdint.h>

// static const json_field_t schema[] = {
//   {"actuatorId", JSON_TYPE_NUMBER, true},
//   {"enable",     JSON_TYPE_BOOL,   true},
// };

DECLARE_ACTION_HANDLER(led_handler) {
  const char *err_field = NULL;
  // if (!json_validate(params, schema, 2, &err_field)) {
  //   if (out_result) *out_result = build_error_fmt("Invalid field: %s", err_field);
  //   return -1;
  // }

  double actuator_id = 0;
  bool enable = false;

  json_get_number(params, "actuatorId", &actuator_id);
  json_get_bool(params, "enable", &enable);

  // map actuator_id -> gpio pin
  // const actuator_capability_t *meta = find_actuator_meta((uint8_t)actuator_id);
  // if (meta == NULL) {
  //   if (out_result)
  //     *out_result = build_error_fmt("actuator with id %#02x not found", (uint8_t)actuator_id);
  //   return -1;
  // }

  // gpio_set_level(meta->pin, enable ? 1 : 0);
  // if (out_result) *out_result = build_success();
  return 0;
}
