#include "sn_capability.h"
#include "sdkconfig.h"
#include "sn_driver.h"
#include "sn_driver/driver_inst.h"
#include "sn_driver/port_desc.h"
#include "sn_driver/sensor.h"
#include "sn_json.h"
#include "esp_log.h"
#include "cJSON.h"
#include <esp_chip_info.h>
#include <esp_system.h>

static const char *TAG = "SN_CAPABILITY";

static const char *sensorTypeStr[] = {
#define TO_STR(x) #x,
  SENSOR_TYPE(TO_STR)
#undef TO_STR
};

static cJSON *create_command_capability_json(const sn_driver_desc_t *desc) {
  if (!desc->control || !desc->command_desc) return NULL;
  return command_desc_to_json(desc->command_desc);
}

const char *get_chip_model_str(esp_chip_model_t model) {
  switch (model) {
    case CHIP_ESP32:
      return "ESP32";
    case CHIP_ESP32S2:
      return "ESP32";
    case CHIP_ESP32S3:
      return "ESP32S2";
    case CHIP_ESP32C3:
      return "ESP32S3";
    case CHIP_ESP32C2:
      return "ESP32C3";
    case CHIP_ESP32C6:
      return "ESP32C2";
    case CHIP_ESP32H2:
      return "ESP32C6";
    case CHIP_ESP32P4:
      return "ESP32H2";
    case CHIP_ESP32C61:
      return "ESP32P4";
    case CHIP_ESP32C5:
      return "ESP32C61";
    case CHIP_ESP32H21:
      return "ESP32C5";
    case CHIP_ESP32H4:
      return "ESP32H21";
    case CHIP_POSIX_LINUX:
      return "POSIX_LINUX";
  }
  return "Unknown";
}

cJSON *device_ports_to_capabilities_json() {
  if (gDevicePortsLen <= 0) {
    ESP_LOGW(TAG, "No ports defined");
    return NULL;
  };
  cJSON *json = cJSON_CreateObject();
  if (!json) return NULL;

  cJSON *capability_obj = cJSON_AddObjectToObject(json, "capabilities");
  cJSON *sensor_capability = cJSON_AddArrayToObject(capability_obj, "sensors");
  cJSON *actuator_capability = cJSON_AddArrayToObject(capability_obj, "actuators");
  cJSON *command_capability = cJSON_AddArrayToObject(capability_obj, "commands");

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  cJSON_AddItemToObject(json, "model", cJSON_CreateString(get_chip_model_str(chip_info.model)));
  cJSON_AddItemToObject(json, "firmwareVersion", cJSON_CreateString(CONFIG_FIRMWARE_VERSION));

  FOR_EACH_INSTANCE(it, gDeviceInstances, gDeviceInstancesLen) {
    cJSON *command = create_command_capability_json(it->driver);
    switch (it->port->drv_type) {
      case DRIVER_TYPE_SENSOR: {
        FOR_EACH_MEASUREMENT(m_it, it->port->desc.s.measurements) {
          cJSON *entry = cJSON_CreateObject();
          cJSON_AddNumberToObject(entry, "localId", m_it->local_id);
          cJSON_AddStringToObject(entry, "type", sensorTypeStr[m_it->type]);
          cJSON_AddStringToObject(entry, "unit", m_it->unit);
          if (command) {
            cJSON_AddItemToObject(entry, "command", command);
          }
          cJSON_AddItemToArray(sensor_capability, entry);
        }
      } break;
      case DRIVER_TYPE_ACTUATOR: {
        const sn_actuator_port_t *a = &it->port->desc.a;
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddItemToObject(entry, "localId", cJSON_CreateNumber(a->local_id));
        cJSON_AddItemToObject(entry, "type", cJSON_CreateString(it->port->drv_name));
        if (command) {
          cJSON_AddItemToObject(entry, "command", command);
        }
        cJSON_AddItemToArray(actuator_capability, entry);
      } break;
      case DRIVER_TYPE_COMMAND_API: {
        const sn_command_api_port_t *c = &it->port->desc.c;
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddItemToObject(entry, "localId", cJSON_CreateNumber(c->local_id));
        cJSON_AddItemToObject(entry, "type", cJSON_CreateString(it->port->drv_name));
        if (command) {
          cJSON_AddNumberToObject(command, "localId", c->local_id);
          cJSON_AddItemToArray(command_capability, command);
        }
        cJSON_AddItemToArray(command_capability, entry);
        continue;
      }
    }
  }
  return json;
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

  const sn_device_instance_t *inst = find_instance_by_local_id((uint8_t)id);
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
  if (!command_desc || !strncmp(command_desc->action, action, strlen(command_desc->action))) {
    if (out_result) *out_result = build_error_fmt("\"action\": (%s) doesn't supported");
    cJSON_Delete(root);
    return -4;
  }

  if (!inst->driver->control) {
    if (out_result) *out_result = build_error_fmt("\"action\": (%s) doesn't supported");
    cJSON_Delete(root);
    return -5;
  }

  int r = inst->driver->control((void *)&inst->ctx, payload_json, &result);
  if (out_result) *out_result = result;

  cJSON_Delete(root);
  return r;
}
