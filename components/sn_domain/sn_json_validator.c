#include "sn_json_validator.h"
#include <stdarg.h>
#include <stdio.h>

bool json_validate(
  const cJSON *root, const json_field_t *schema, size_t field_count, const char **error_msg
) {
  if (!cJSON_IsObject(root)) {
    if (error_msg) *error_msg = "Root is not a JSON object";
    return false;
  }

  for (int i = 0; i < field_count; ++i) {
    const json_field_t *f = &schema[i];
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, f->key);

    if (!item) {
      if (f->required) {
        if (error_msg) *error_msg = f->key;
        return false; // missing required field
      }
      continue; // optional field missing — okay
    }

    bool type_ok = false;
    switch (f->type) {
      case JSON_TYPE_BOOL:
        type_ok = cJSON_IsBool(item);
        break;
      case JSON_TYPE_NUMBER:
        type_ok = cJSON_IsNumber(item);
        break;
      case JSON_TYPE_STRING:
        type_ok = cJSON_IsString(item);
        break;
      case JSON_TYPE_OBJECT:
        type_ok = cJSON_IsObject(item);
        break;
      case JSON_TYPE_ARRAY:
        type_ok = cJSON_IsArray(item);
        break;
    }

    if (!type_ok) {
      if (error_msg) *error_msg = f->key;
      return false;
    }
  }

  return true;
}

cJSON *build_success() {
  cJSON *result = cJSON_CreateObject();
  if (!result) return NULL;
  cJSON_AddItemToObject(result, "status", cJSON_CreateString("success"));
  return result;
}

cJSON *build_error_fmt(const char *reason, ...) {
  char buf[128];
  if (reason == NULL) {
    reason = "Unknown error";
  }
  va_list args;
  va_start(args, reason);
  vsnprintf(buf, sizeof(buf), reason, args);
  va_end(args);

  buf[sizeof(buf) - 1] = '\0'; // ensure null-termination

  cJSON *result = cJSON_CreateObject();
  if (!result) return NULL;
  cJSON_AddItemToObject(result, "message", cJSON_CreateString(buf));
  cJSON_AddItemToObject(result, "status", cJSON_CreateString("error"));
  return result;
}

bool json_get_bool(const cJSON *root, const char *key, bool *out_val) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
  if (!cJSON_IsBool(item)) return false;
  *out_val = cJSON_IsTrue(item);
  return true;
}

bool json_get_number(const cJSON *root, const char *key, double *out_val) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
  if (!cJSON_IsNumber(item)) return false;
  *out_val = cJSON_GetNumberValue(item);
  return true;
}

bool json_get_object(const cJSON *root, const char *key, const cJSON **out_val) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
  if (!cJSON_IsObject(item)) return false;
  *out_val = item;
  return true;
}

bool json_get_string(const cJSON *root, const char *key, const char **out_val) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
  if (!cJSON_IsString(item)) return false;
  *out_val = item->valuestring;
  return true;
}
