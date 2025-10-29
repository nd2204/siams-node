#include "sn_device.h"
#include "sn_error.h"
#include "sn_json_validator.h"
#include "cJSON.h"
#include <assert.h>
#include <string.h>

static cJSON *sensor_capabilities_to_json(const sensor_capability_t *const s) {
  assert(s);
  cJSON *object = cJSON_CreateObject();
  cJSON_AddNumberToObject(object, "localId", s->localId);
  cJSON_AddStringToObject(object, "unit", s->unit);
  cJSON_AddStringToObject(object, "type", s->type);
  return object;
}

static cJSON *actuator_capabilities_to_json(const actuator_capability_t *const a) {
  assert(a);
  cJSON *object = cJSON_CreateObject();
  cJSON_AddNumberToObject(object, "localId", a->localId);
  cJSON_AddStringToObject(object, "type", a->type);
  return object;
}

static cJSON *command_capabilities_to_json(const command_capability_t *const c) {
  assert(c);
  cJSON *object = cJSON_CreateObject();
  cJSON_AddStringToObject(object, "action", c->action);
  if (c->params && c->params_len > 0) {
    cJSON *array = cJSON_AddArrayToObject(object, "params");
    for (int i = 0; i < c->params_len; i++) {
      cJSON_AddItemToArray(array, cJSON_CreateString(c->params[i]));
    }
  }
  return object;
}

cJSON *device_capabilities_to_json(const device_capabilities_t *const c) {
  if (c) {
    cJSON *json = cJSON_CreateObject();
    cJSON *array = NULL;
    array = cJSON_AddArrayToObject(json, "sensors");
    for (int i = 0; i < c->slen; i++) {
      cJSON_AddItemToArray(array, sensor_capabilities_to_json(&c->sensors[i]));
    }
    array = cJSON_AddArrayToObject(json, "actuators");
    for (int i = 0; i < c->alen; i++) {
      cJSON_AddItemToArray(array, actuator_capabilities_to_json(&c->actuators[i]));
    }
    array = cJSON_AddArrayToObject(json, "commands");
    for (int i = 0; i < c->clen; i++) {
      cJSON_AddItemToArray(array, command_capabilities_to_json(&c->commands[i]));
    }
    return json;
  }
  return NULL;
}

const sensor_capability_t *find_sensor_meta(local_id_t sensor_id) {
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    if (gSensorCapabilities[i].localId == sensor_id) {
      return &gSensorCapabilities[i];
    }
  }
  return NULL;
}

const actuator_capability_t *find_actuator_meta(local_id_t actuator_id) {
  for (int i = 0; i < ACTUATOR_COUNT; ++i) {
    if (gActuatorCapabilities[i].localId == actuator_id) {
      return &gActuatorCapabilities[i];
    }
  }
  return NULL;
}

const command_handler_entry_t *find_command_handler(local_id_t cmd_id) {
  for (int i = 0; i < HANDLER_COUNT; ++i) {
    if (gCommandHandlers[i].localId == cmd_id) {
      return &gCommandHandlers[i];
    }
  }
  return NULL;
}

const command_capability_t *find_command_meta(const char *action) {
  if (!action) return NULL;
  for (int i = 0; i < COMMAND_COUNT; ++i) {
    if (strcmp(gCommandCapabilities[i].action, action) == 0) {
      return &gCommandCapabilities[i];
    }
  }
  return NULL;
}

// pseudo: command JSON format from backend:
// {
//   "action": "set_pump_state",
//   "params": { "actuatorId": 5, "enable": true }
// }
int dispatch_command(const char *payload_json, cJSON **out_result) {
  cJSON *root = cJSON_Parse(payload_json);
  if (!root) {
    if (out_result) *out_result = build_error_fmt("payload is not in a correct json format");
    return -1;
  }

  static json_field_t schema[] = {
    {"action", JSON_TYPE_STRING, true},
    {"params", JSON_TYPE_OBJECT, true},
  };

  const char *err_field = NULL;
  if (!json_validate(root, schema, sizeof(schema) / sizeof(json_field_t), &err_field)) {
    if (out_result) *out_result = build_error_fmt("Invalid or missing field: %s", err_field);
    cJSON_Delete(root);
    return -2;
  }

  const char *action = NULL;
  const cJSON *params = NULL;

  json_get_string(root, "action", &action);
  json_get_object(root, "params", &params);

  const command_capability_t *meta = find_command_meta(action);
  if (!meta) {
    if (out_result) *out_result = build_error_fmt("Action not supported: %s", action);
    cJSON_Delete(root);
    return -5;
  }

  // 2) validate params presence (based on meta)
  // parameters not provided but the action required
  // ignore the params if otherway around
  if (!params && meta->params_len > 0) {
    if (out_result) *out_result = build_error_fmt("Invalid operation: Empty parameters");
    cJSON_Delete(root);
    return -6;
  }

  const command_handler_entry_t *command = find_command_handler(meta->localId);
  if (!command) {
    if (out_result) *out_result = build_error_fmt("Handler not found for action: %s", action);
    cJSON_Delete(root);
    return -7;
  }

  return command->handler(params, out_result);
}
