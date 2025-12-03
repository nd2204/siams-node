#include "sn_capability.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "sn_driver.h"
#include "sn_driver/driver_inst.h"
#include "sn_driver/port_desc.h"
#include "sn_driver/sensor.h"
#include "sn_json.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_mac.h"
#include <esp_efuse.h>
#include <esp_chip_info.h>
#include <esp_system.h>
#include <esp_efuse.h>

static const char *TAG = "SN_CAPABILITY";

static const char *sensorTypeStr[] = {
#define TO_ENUM_STR(x) TO_STR(x),
  SENSOR_TYPE(TO_ENUM_STR)
#undef TO_ENUM_STR
};

#define STRINGIZE(x) TO_STR(x)

#define FIRMWARE_VERSION CONFIG_FIRMWARE_VERSION
#if !defined(FIRMWARE_VERSION)
#error "CONFIG_FIRMWARE_VERSION required"
#endif

static cJSON *create_command_capability_json(const sn_driver_desc_t *desc) {
  if (!desc->control || !desc->command_desc) return NULL;
  return command_desc_to_json(desc->command_desc);
}

/* Result payload sample:
 * {
 *   model: "ESP32",
 *   fw_ver: "v1.0.0",
 *   hw_id: ""
 *   sensors: [
 *     {localId: 1, name: 'temp-1', type: 'dht11', unit: "C"},
 *     ...
 *   ],
 *   actuators: [
 *     {localId: 2, name: 'pump-1', type: 'PUMP'},
 *     ...
 *   ],
 *   commands: [
 *     {localId: 2, commands: [
 *       {action: "control_relay", params: [
 *         { name: "enable", type: "boolean" },
 *         { name: "duration_sec", type: "int" }
 *       ]}
 *     ]}
 *   ]
 * }
 */
cJSON *device_ports_to_capabilities_json() {
  if (gDevicePortsLen <= 0) {
    ESP_LOGW(TAG, "No ports defined");
    return NULL;
  };
  cJSON *json = cJSON_CreateObject();
  if (!json) return NULL;

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  const char *model = get_chip_model_str(chip_info.model);

  char mac_str[64];
  get_hwid(mac_str, sizeof(mac_str));

  cJSON_AddItemToObject(json, "model", cJSON_CreateString(model));
  cJSON_AddItemToObject(json, "fw_ver", cJSON_CreateString("v1.0.0"));
  cJSON_AddItemToObject(json, "hw_id", cJSON_CreateString(mac_str));
  // TODO: Add a GPS module?
  cJSON_AddNumberToObject(json, "lat", 21.047324478422933);
  cJSON_AddNumberToObject(json, "lon", 105.78543402753904);
  // TODO: support ecdsa if device support secure element
  cJSON_AddItemToObject(json, "signing", cJSON_CreateString("hmac"));
  // cJSON_AddItemToObject(json, "pubkey", cJSON_CreateString(""));

  cJSON *sensor_capability = cJSON_AddArrayToObject(json, "sensors");
  cJSON *actuator_capability = cJSON_AddArrayToObject(json, "actuators");
  cJSON *command_capability = cJSON_AddArrayToObject(json, "commands");
  FOR_EACH_INSTANCE(it, gDeviceInstances, gDeviceInstancesLen) {
    cJSON *command_obj = cJSON_CreateObject();
    cJSON *command_array = cJSON_AddArrayToObject(command_obj, "commands");
    cJSON *command = create_command_capability_json(it->driver);
    if (command) cJSON_AddItemToArray(command_array, command);

    switch (it->port->drv_type) {
      case DRIVER_TYPE_SENSOR: {
        FOR_EACH_MEASUREMENT(m_it, it->port->desc.s.measurements) {
          cJSON *entry = cJSON_CreateObject();
          cJSON_AddNumberToObject(entry, "localId", m_it->local_id);
          cJSON_AddStringToObject(entry, "type", sensorTypeStr[m_it->type]);
          cJSON_AddItemToObject(entry, "name", cJSON_CreateString(it->port->port_name));
          cJSON_AddStringToObject(entry, "unit", m_it->unit);
          cJSON_AddItemToArray(sensor_capability, entry);
          if (command) {
            cJSON_AddNumberToObject(command_obj, "local_id", m_it->local_id);
            cJSON_AddItemToArray(command_capability, command_obj);
          }
        }
      } break;
      case DRIVER_TYPE_ACTUATOR: {
        const sn_actuator_port_t *a = &it->port->desc.a;
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddItemToObject(entry, "localId", cJSON_CreateNumber(a->local_id));
        cJSON_AddItemToObject(entry, "name", cJSON_CreateString(it->port->port_name));
        cJSON_AddItemToObject(entry, "type", cJSON_CreateString(it->port->drv_name));
        cJSON_AddItemToArray(actuator_capability, entry);
        if (command) {
          cJSON_AddNumberToObject(command_obj, "local_id", a->local_id);
          cJSON_AddItemToArray(command_capability, command_obj);
        }
      } break;
      case DRIVER_TYPE_COMMAND_API: {
        const sn_command_api_port_t *c = &it->port->desc.c;
        if (command) {
          cJSON_AddItemToObject(command_obj, "local_id", cJSON_CreateNumber(c->local_id));
          cJSON_AddItemToObject(command_obj, "name", cJSON_CreateString(it->port->port_name));
          cJSON_AddItemToObject(command_obj, "type", cJSON_CreateString(it->port->drv_name));
          cJSON_AddItemToArray(command_capability, command_obj);
        }
        continue;
      }
    }
  }
  return json;
}

int sn_dispatch_command_struct(const sn_command_t *command, cJSON **out_result) {
  cJSON *result = NULL;

  if (command->local_id > LOCAL_ID_MAX || command->local_id < LOCAL_ID_MIN) {
    if (out_result)
      *out_result = build_error_fmt(
        "Invalid field \"localId\": %d (min: %d) (max: %d)", command->local_id, LOCAL_ID_MIN,
        LOCAL_ID_MAX
      );
    return -2;
  }

  const sn_device_instance_t *inst = sn_find_instance_by_local_id((uint8_t)command->local_id);
  if (!inst) {
    if (out_result) {
      *out_result =
        build_error_fmt("Cannot found any command associated with localId=%d", command->local_id);
    }
    return -2;
  }

  if (!command->action) {
    if (out_result) *out_result = build_error_fmt("Invalid or missing field \"action\"");
    return -3;
  }

  const sn_command_desc_t *command_desc = inst->driver->command_desc;
  if (!command_desc
      || strncmp(command_desc->action, command->action, strlen(command_desc->action)) != 0) {
    if (out_result)
      *out_result = build_error_fmt("\"action\": \"%s\" unsupported", command->action);
    return -4;
  }

  if (!inst->driver->control) {
    if (out_result)
      *out_result = build_error_fmt("\"action\": \"%s\" missing control callback", command->action);
    return -5;
  }

  cJSON *params = cJSON_Parse(command->params_json);
  int r = inst->driver->control((void *)&inst->ctx, params, &result);

  if (r != ESP_OK) {
    ESP_LOGE(TAG, "%s Encountered an error (%s)", inst->port->port_name, esp_err_to_name(r));
  }

  if (params) cJSON_Delete(params);
  if (out_result) *out_result = result;
  return r;
}

// pseudo: command JSON format from backend:
// {
//   "localId": number,
//   "action": "set_pump_state",
//   "params": { "actuatorId": 5, "enable": true }
// }
int dispatch_command(const char *payload_json, cJSON **out_result) {
  cJSON *root = cJSON_Parse(payload_json);
  cJSON *result = NULL;

  if (!root) {
    if (out_result) *out_result = build_error_fmt("payload is not in a correct json format");
    return -1;
  }

  int id = 0;
  if (!json_get_int(root, "localId", &id)) {
    if (out_result) *out_result = build_error_fmt("Invalid or missing field \"localId\"");
    cJSON_Delete(root);
    return -2;
  }

  const sn_device_instance_t *inst = sn_find_instance_by_local_id((uint8_t)id);
  if (!inst) {
    if (out_result)
      *out_result =
        build_error_fmt("Cannot found any command associated with localId=%d", (uint8_t)id);
    cJSON_Delete(root);
    return -2;
  }

  const char *action = NULL;
  if (!json_get_string(root, "action", &action)) {
    if (out_result) *out_result = build_error_fmt("Invalid or missing field \"action\"");
    cJSON_Delete(root);
    return -3;
  }

  const sn_command_desc_t *command_desc = inst->driver->command_desc;
  if (!command_desc || strncmp(command_desc->action, action, strlen(command_desc->action)) != 0) {
    if (out_result) *out_result = build_error_fmt("\"action\": \"%s\" unsupported", action);
    cJSON_Delete(root);
    return -4;
  }

  if (!inst->driver->control) {
    if (out_result)
      *out_result = build_error_fmt("\"action\": \"%s\" missing control callback", action);
    cJSON_Delete(root);
    return -5;
  }

  cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
  int r = inst->driver->control((void *)&inst->ctx, params, &result);

  if (r != ESP_OK) {
    ESP_LOGE(TAG, "%s Encountered an error (%s)", inst->port->port_name, esp_err_to_name(r));
  }
  if (out_result) *out_result = result;

  cJSON_Delete(root);
  return r;
}
