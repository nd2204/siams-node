#include "sn_capability.h"
#include "sn_driver.h"
#include "sn_driver/port_desc.h"
#include "sn_driver/sensor.h"
#include "sn_json.h"
#include "esp_log.h"
#include "cJSON.h"

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

cJSON *device_ports_to_capabilities_json() {
  if (gDevicePortsLen <= 0) {
    ESP_LOGW(TAG, "No ports defined");
    return NULL;
  };
  cJSON *json = cJSON_CreateObject();
  cJSON *sensor_capability = cJSON_CreateArray();
  cJSON *actuator_capability = cJSON_CreateArray();
  cJSON *command_capability = cJSON_CreateArray();

  cJSON_AddItemToObject(json, "sensors", sensor_capability);
  cJSON_AddItemToObject(json, "actuators", actuator_capability);
  cJSON_AddItemToObject(json, "commands", command_capability);

  FOR_EACH_INSTANCE(it, gDeviceInstances, gDeviceInstancesLen) {
    switch (it->port->drv_type) {
      case DRIVER_TYPE_SENSOR: {
        FOR_EACH_MEASUREMENT(m_it, it->port->desc.s.measurements) {
          cJSON *entry = cJSON_CreateObject();
          cJSON_AddNumberToObject(entry, "localId", m_it->local_id);
          cJSON_AddStringToObject(entry, "type", sensorTypeStr[m_it->type]);
          cJSON_AddStringToObject(entry, "unit", m_it->unit);
          cJSON *command = create_command_capability_json(it->driver);
          if (command) {
            cJSON_AddNumberToObject(command, "localId", m_it->local_id);
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
        cJSON *command = create_command_capability_json(it->driver);
        if (command) {
          cJSON_AddNumberToObject(command, "localId", a->local_id);
          cJSON_AddItemToObject(entry, "command", command);
        }
        cJSON_AddItemToArray(actuator_capability, entry);
      } break;
      case DRIVER_TYPE_COMMAND_API: {
        const sn_command_api_port_t *c = &it->port->desc.c;
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddItemToObject(entry, "localId", cJSON_CreateNumber(c->local_id));
        cJSON_AddItemToObject(entry, "type", cJSON_CreateString(it->port->drv_name));
        cJSON *command = create_command_capability_json(it->driver);
        if (command) {
          cJSON_AddNumberToObject(command, "localId", c->local_id);
          cJSON_AddItemToObject(entry, "command", command);
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
//   "action": "set_pump_state",
//   "params": { "actuatorId": 5, "enable": true }
// }
int dispatch_command(const char *payload_json, cJSON **out_result) {
  cJSON *root = cJSON_Parse(payload_json);
  if (!root) {
    if (out_result) *out_result = build_error_fmt("payload is not in a correct json format");
    return -1;
  }
  return 0;
}
