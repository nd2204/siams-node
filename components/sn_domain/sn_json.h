#ifndef SN_JSON_H
#define SN_JSON_H

#include "cJSON.h"
#include "sn_driver/sensor.h"
#include <stdbool.h>

typedef enum { PTYPE_INT = 0, PTYPE_NUMBER, PTYPE_BOOL, PTYPE_STRING } ptype_t;

typedef struct {
  const char *name;

  // NULL-terminated string enums
  const char **enum_values;

  // use only for numeric types
  double min;
  double max;

  ptype_t type; // PTYPE_INT, PTYPE_BOOL, ...
  bool required;
} sn_param_desc_t;

typedef struct {
  const char *action;
  const sn_param_desc_t *params;
} sn_command_desc_t;

#define FOREACH_PARAMS_DESC(it, params)                                                            \
  for (const sn_param_desc_t *it = (params); it && (it->name); it++)

#define FOREACH_ENUM_VALUES(it, enums) for (const char **it = (it); it; it++)

cJSON *command_desc_to_json(const sn_command_desc_t *desc);

typedef struct {
  local_id_t local_id;
  const char *action;
  const char *params_json;
} sn_command_t;

#define COMMAND_NULL_ENTRY                                                                         \
  (sn_command_t) { .action = NULL, .local_id = 0, .params_json = NULL }

#define FOREACH_COMMAND(it, commands)                                                              \
  for (const sn_command_t *it = (commands); it && (it->action); it++)

cJSON *sn_command_to_payload_json(const sn_command_t *command);

// Utils
bool validate_params_json(const sn_param_desc_t *desc, const cJSON *params, cJSON **err_out);

cJSON *build_error_fmt(const char *reason, ...);

cJSON *build_success_fmt(const char *fmt, ...);

cJSON *sensor_reading_to_json_obj(const sn_sensor_reading_t *m);

bool json_get_bool(const cJSON *root, const char *key, bool *out_val);

bool json_get_number(const cJSON *root, const char *key, double *out_val);

bool json_get_int(const cJSON *root, const char *key, int *out_val);

bool json_get_object(const cJSON *root, const char *key, cJSON **out_val);

bool json_get_string(const cJSON *root, const char *key, const char **out_val);

#endif // !SN_JSON_H
