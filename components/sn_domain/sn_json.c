#include "sn_json.h"
#include "cJSON.h"
#include "sn_sntp.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

cJSON *json_create_string_fmt(const char *fmt, ...) {
  char buf[256];
  if (fmt == NULL) return NULL;
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  buf[sizeof(buf) - 1] = '\0'; // ensure null-termination
  return cJSON_CreateString(buf);
}

bool validate_params_json(const sn_param_desc_t *desc, cJSON *params, cJSON **err_out) {
  if (!desc || !params) return false;
  for (const sn_param_desc_t *pd = desc; pd && pd->name; ++pd) {
    cJSON *it = cJSON_GetObjectItemCaseSensitive(params, pd->name);
    if (!it) {
      if (pd->required) {
        if (err_out) *err_out = build_error_fmt("missing required param '%s'", pd->name);
        return false;
      } else
        continue;
    }
    switch (pd->type) {
      case PTYPE_INT:
        if (!cJSON_IsNumber(it) || (it->valuedouble != (double)(it->valueint))) {
          if (err_out) *err_out = build_error_fmt("param '%s' must be integer", pd->name);
          return false;
        }
        if (pd->min != pd->max) { // treat min/max default values carefully
          if (it->valueint < (int)pd->min || it->valueint > (int)pd->max) {
            if (err_out) *err_out = build_error_fmt("param '%s' out of range", pd->name);
            return false;
          }
        }
        break;
      case PTYPE_NUMBER:
        if (!cJSON_IsNumber(it)) {
          if (err_out) *err_out = build_error_fmt("param '%s' must be number", pd->name);
          return false;
        }
        if (pd->min != pd->max) {
          if (it->valuedouble < pd->min || it->valuedouble > pd->max) {
            if (err_out) *err_out = build_error_fmt("param '%s' out of range", pd->name);
            return false;
          }
        }
        break;
      case PTYPE_BOOL:
        if (!cJSON_IsBool(it)) {
          if (err_out) *err_out = build_error_fmt("param '%s' must be boolean", pd->name);
          return false;
        }
        break;
      case PTYPE_STRING:
        if (!cJSON_IsString(it)) {
          if (err_out) *err_out = build_error_fmt("param '%s' must be string", pd->name);
          return false;
        }
        if (pd->enum_values) {
          bool found = false;
          for (const char **ev = pd->enum_values; ev && *ev; ++ev) {
            if (strcmp(*ev, it->valuestring) == 0) {
              found = true;
              break;
            }
          }
          if (!found) {
            if (err_out) *err_out = build_error_fmt("param '%s' unexpected value", pd->name);
            return false;
          }
        }
        break;
    }
  }
  // check for extra/unexpected props if needed
  return true;
}

/*
 * returns an command capability json with this structure:
 * {
 *   "action": "action-name",
 *   "params": {  // (optional)
 *     "arg1": {
 *       "type": "string",
 *       "enums": ["a", "b"] // (optional)
 *     },
 *     "arg2": { "type": "number"},
 *     ...
 *   },
 * }
 */

static const char *ptype_str[] = {"int", "number", "boolean", "string"};

cJSON *command_desc_to_json(const sn_command_desc_t *desc) {
  if (!desc) return NULL;
  cJSON *schema = cJSON_CreateObject();
  if (!schema) return NULL;

  cJSON_AddStringToObject(schema, "action", desc->action);

  // Skip if no params
  if (desc->params_count < 1) {
    return schema;
  }
  const sn_param_desc_t *params = desc->params;
  cJSON *paramsJsonObj = cJSON_AddObjectToObject(schema, "params");
  for (int i = 0; i < desc->params_count; i++) {
    cJSON *arg = cJSON_AddObjectToObject(paramsJsonObj, params[i].name);
    cJSON_AddStringToObject(arg, "type", ptype_str[params[i].type]);
    const char **it = params[i].enum_values;
    if (*it) {
      cJSON *enum_arr = cJSON_AddArrayToObject(arg, "enums");
      for (; *it; i++) {
        cJSON_AddItemToArray(enum_arr, cJSON_CreateString(*it));
      }
    }
  }
  return schema;
}

cJSON *sensor_reading_to_json_obj(const sn_sensor_reading_t *m) {
  cJSON *mo = cJSON_CreateObject();
  if (!mo || !m) {
    return NULL;
  }
  cJSON_AddNumberToObject(mo, "localId", m->local_id);
  cJSON_AddNumberToObject(mo, "value", m->value);
  char buffer[128];
  sn_timestamp_to_iso8601(m->ts, buffer, sizeof(buffer));
  cJSON_AddStringToObject(mo, "ts", buffer);

  return mo;
}

cJSON *build_success_fmt(const char *fmt, ...) {
  cJSON *result = cJSON_CreateObject();

  if (fmt != NULL) {
    va_list args;
    va_start(args, fmt);
    cJSON_AddItemToObject(result, "message", json_create_string_fmt(fmt, args));
    va_end(args);
  }

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
