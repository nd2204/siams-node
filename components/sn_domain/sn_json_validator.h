#ifndef SN_JSON_VALIDATOR_H
#define SN_JSON_VALIDATOR_H

#include "cJSON.h"
#include <stdbool.h>

typedef enum {
  JSON_TYPE_BOOL,
  JSON_TYPE_NUMBER,
  JSON_TYPE_STRING,
  JSON_TYPE_OBJECT,
  JSON_TYPE_ARRAY
} json_type_t;

typedef struct {
  const char *key;
  json_type_t type;
  bool required;
} json_field_t;

bool json_validate(
  const cJSON *root, const json_field_t *schema, size_t field_count, const char **error_msg
);

cJSON *build_error_fmt(const char *reason, ...);

cJSON *build_success();

bool json_get_bool(const cJSON *root, const char *key, bool *out_val);

bool json_get_number(const cJSON *root, const char *key, double *out_val);

bool json_get_object(const cJSON *root, const char *key, const cJSON **out_val);

bool json_get_string(const cJSON *root, const char *key, const char **out_val);

#endif // !SN_JSON_VALIDATOR_H
