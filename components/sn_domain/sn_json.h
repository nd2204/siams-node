#ifndef SN_JSON_H
#define SN_JSON_H

#include "cJSON.h"
#include "sn_driver/sensor.h"
#include <stdbool.h>

typedef enum { PTYPE_INT = 0, PTYPE_NUMBER, PTYPE_BOOL, PTYPE_STRING } ptype_t;

typedef struct {
  const char *name;
  const char **enum_values; // NULL-terminated if used for strings
  double min;               // use only for numeric
  double max;
  ptype_t type;
  bool required;
} sn_param_desc_t;

typedef struct {
  const char *action;
  const sn_param_desc_t *params;
  size_t params_count;
} sn_command_desc_t;

bool validate_params_json(const sn_param_desc_t *desc, const cJSON *params, cJSON **err_out);

cJSON *command_desc_to_json(const sn_command_desc_t *desc);

cJSON *build_error_fmt(const char *reason, ...);

cJSON *build_success_fmt(const char *fmt, ...);

cJSON *sensor_reading_to_json_obj(const sn_sensor_reading_t *m);

bool json_get_bool(const cJSON *root, const char *key, bool *out_val);

bool json_get_number(const cJSON *root, const char *key, double *out_val);

bool json_get_int(const cJSON *root, const char *key, int *out_val);

bool json_get_object(const cJSON *root, const char *key, cJSON **out_val);

bool json_get_string(const cJSON *root, const char *key, const char **out_val);

#endif // !SN_JSON_H
