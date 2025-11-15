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

const char *cjson_type_to_name(int type) {
  // clang-format off
  if (type & cJSON_Invalid) { return "Invalid"; }
  if (type & cJSON_False  ) { return "False"; }
  if (type & cJSON_True   ) { return "True"; }
  if (type & cJSON_NULL   ) { return "NULL"; }
  if (type & cJSON_Number ) { return "Number"; }
  if (type & cJSON_String ) { return "String"; }
  if (type & cJSON_Array  ) { return "Array"; }
  if (type & cJSON_Object ) { return "Object"; }
  if (type & cJSON_Raw    ) { return "Raw"; }
  return "Error";
  // clang-format on
}

bool validate_params_json(const sn_param_desc_t *desc, const cJSON *params, cJSON **err_out) {
  if (!desc || !params) return false;
  for (const sn_param_desc_t *pd = desc; pd && pd->name; ++pd) {
    cJSON *it = cJSON_GetObjectItemCaseSensitive(params, pd->name);
    if (!it) {
      if (pd->required) {
        if (err_out) *err_out = build_error_fmt("missing required param '%s'", pd->name);
        return false;
      }
      continue;
    } else {
      switch (pd->type) {
        case PTYPE_INT:
          if (!cJSON_IsNumber(it) || (it->valuedouble != (double)(it->valueint))) {
            if (err_out)
              *err_out = build_error_fmt(
                "param '%s' (%s) must be integer", cjson_type_to_name(it->type), pd->name
              );
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
            if (err_out)
              *err_out = build_error_fmt(
                "param '%s' (%s) must be number", cjson_type_to_name(it->type), pd->name
              );
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
            if (err_out)
              *err_out = build_error_fmt(
                "param '%s' (%s) must be boolean", cjson_type_to_name(it->type), pd->name
              );
            return false;
          }
          break;
        case PTYPE_STRING:
          if (!cJSON_IsString(it)) {
            if (err_out)
              *err_out = build_error_fmt(
                "param '%s' (%s) must be string %s", cjson_type_to_name(it->type), pd->name
              );
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
  }
  // check for extra/unexpected props if needed
  return true;
}

/*
 * returns an command capability json with this structure:
 * {
 *   "action": "action-name",
 *   "params": [  // (optional)
 *     {
 *       "name":"arg1",
 *       "type": "string",
 *       "enums": ["a", "b"] // (optional)
 *     },
 *     {
 *       "name": "arg2",
 *       "type": "number"
 *     }
 *     ...
 *   ],
 * }
 */

static const char *ptype_str[] = {"int", "number", "boolean", "string"};

cJSON *sn_command_to_payload_json(const sn_command_t *command) {
  if (!command) return NULL;
  cJSON *params_obj = cJSON_Parse(command->params_json);
  if (!params_obj) return NULL;

  cJSON *payload_obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(payload_obj, "localId", command->local_id);
  cJSON_AddStringToObject(payload_obj, "action", command->action);
  cJSON_AddItemToObject(payload_obj, "params", params_obj);
  return payload_obj;
}

cJSON *command_desc_to_json(const sn_command_desc_t *desc) {
  if (!desc) return NULL;
  cJSON *schema = cJSON_CreateObject();
  if (!schema) return NULL;

  cJSON_AddStringToObject(schema, "action", desc->action);

  // Skip if no params
  if (!desc->params || !desc->params->name) {
    return schema;
  }
  cJSON *params_obj = cJSON_AddArrayToObject(schema, "params");
  FOREACH_PARAMS_DESC(it, desc->params) {
    cJSON *arg_obj = cJSON_CreateObject();

    cJSON_AddStringToObject(arg_obj, "name", it->name);
    cJSON_AddStringToObject(arg_obj, "type", ptype_str[it->type]);
    if (it->enum_values) {
      cJSON *enum_arr = cJSON_AddArrayToObject(arg_obj, "enums");
      FOREACH_ENUM_VALUES(it, it->enum_values) {
        cJSON_AddItemToArray(enum_arr, cJSON_CreateString(*it));
      }
    }
    cJSON_AddItemToArray(params_obj, arg_obj);
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
  cJSON_AddNumberToObject(mo, "ts", m->ts);
  return mo;
}

cJSON *build_success_fmt(const char *fmt, ...) {
  cJSON *result = cJSON_CreateObject();
  if (!result) return NULL;
  cJSON_AddItemToObject(result, "status", cJSON_CreateString("success"));

  if (fmt == NULL) {
    return result;
  }

  char buf[128];
  va_list args;
  va_start(args, fmt);
  int written = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  buf[sizeof(buf) - 1] = '\0'; // ensure null-termination

  if (written < 0) {
    cJSON_AddStringToObject(result, "message", "format error");
  } else {
    cJSON_AddStringToObject(result, "message", buf);
  }

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

bool json_get_int(const cJSON *root, const char *key, int *out_val) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
  if (!cJSON_IsNumber(item)) return false;
  *out_val = (int)cJSON_GetNumberValue(item);
  return true;
}

bool json_get_object(const cJSON *root, const char *key, cJSON **out_val) {
  cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
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
