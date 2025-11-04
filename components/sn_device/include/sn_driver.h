#ifndef SN_DEVICE_DRIVER_H
#define SN_DEVICE_DRIVER_H

#include "sn_driver/actuator.h"    // IWYU pragma: export
#include "sn_driver/driver_desc.h" // IWYU pragma: export
#include "sn_driver/driver_inst.h" // IWYU pragma: export
#include "sn_driver/driver_type.h" // IWYU pragma: export
#include "sn_driver/port_desc.h"   // IWYU pragma: export
#include "sn_driver/sensor.h"      // IWYU pragma: export
#include <string.h>

#define MAX_INSTANCES 16

/* --------------------------------------------------------------------
 *  Global registry definition (declared in a .c file)
 * ------------------------------------------------------------------*/

extern const sn_device_port_desc_t gDevicePorts[];
extern const size_t gDevicePortsLen;
extern sn_device_instance_t gDeviceInstances[MAX_INSTANCES];
extern size_t gDeviceInstancesLen;

// --------------------------------------------------------------------------------
// Iteration utilities
// --------------------------------------------------------------------------------

// Generic iterator macro
#define FOR_EACH_DEVICE(it, ports, len)                                                            \
  for (const sn_device_port_desc_t *it = (ports); it < ((ports) + (len)); ++it)

// Iterate only devices of a given role
#define FOR_EACH_DEVICE_ROLE(it, ports, len, role_value)                                           \
  FOR_EACH_DEVICE(it, ports, len)                                                                  \
  if (it->role == role_value)

// Iterate through all online instances
#define FOR_EACH_INSTANCE(it, instances, len)                                                      \
  for (sn_device_instance_t *it = (instances); it < ((instances) + (len)); ++it)

// Iterate only sensors
#define FOR_EACH_SENSOR_INSTANCE(it, instances, len)                                               \
  FOR_EACH_INSTANCE(it, instances, len)                                                            \
  if (it->port && it->port->drv_type == DRIVER_TYPE_SENSOR)

// Iterate only actuators
#define FOR_EACH_ACTUATOR_INSTANCE(it, instances, len)                                             \
  FOR_EACH_INSTANCE(it, instances, len)                                                            \
  if (it->port && it->port->drv_type == DRIVER_TYPE_ACTUATOR)

/* --------------------------------------------------------------------
 *  Lookup utilities
 * ------------------------------------------------------------------*/

// Find by name (case-sensitive)
static inline const sn_device_port_desc_t *find_device_by_name(const char *name) {
  if (!name) return NULL;
  for (size_t i = 0; i < gDevicePortsLen; ++i) {
    const char *devName = NULL;
    if (gDevicePorts[i].drv_type == DRIVER_TYPE_SENSOR) {
      devName = gDevicePorts[i].port_name;
    } else if (gDevicePorts[i].drv_type == DRIVER_TYPE_ACTUATOR) {
      devName = gDevicePorts[i].port_name;
    }
    if (!devName) return NULL;
    if (strcmp(devName, name) == 0) return &gDevicePorts[i];
  }
  return NULL;
}

// Find by local_id
static inline const sn_device_port_desc_t *find_device_by_local_id(uint8_t local_id) {
  FOR_EACH_DEVICE(it, gDevicePorts, gDevicePortsLen) {
    switch (it->drv_type) {
      case DRIVER_TYPE_SENSOR: {
        FOR_EACH_MEASUREMENT(m_it, it->desc.s.measurements) {
          if (m_it->local_id == local_id) {
            return it;
          }
        }
        return NULL;
      } break;
      case DRIVER_TYPE_ACTUATOR: {
        if (it->desc.a.local_id == local_id) return it;
      } break;
      case DRIVER_TYPE_COMMAND_API: {
        if (it->desc.c.local_id == local_id) return it;
      } break;
    }
  }
  return NULL;
}

// Find first by driver_name (useful for shared drivers like dht22)
static inline const sn_device_port_desc_t *find_device_by_driver_name(const char *name) {
  if (!name) return NULL;
  for (size_t i = 0; i < gDevicePortsLen; ++i)
    if (strcmp(gDevicePorts[i].drv_name, name) == 0) return &gDevicePorts[i];
  return NULL;
}

// Count by driver_type
// For capabilities
static inline size_t count_devices_by_driver_type(driver_type_e type) {
  size_t n = 0;
  for (size_t i = 0; i < gDevicePortsLen; ++i)
    if (gDevicePorts[i].drv_type == type) ++n;
  return n;
}

// Find by port name (case-sensitive)
static inline sn_device_instance_t *find_instance_by_name(const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < gDevicePortsLen; ++i) {
    const sn_device_instance_t *inst = &gDeviceInstances[i];
    if (inst->port && strcmp(inst->port->port_name, name) == 0) return (sn_device_instance_t *)inst;
  }
  return NULL;
}

// Debug print
static inline void print_instance(const sn_device_instance_t *inst) {
  if (!inst) {
    ESP_LOGE("DEBUG", "Instance is null");
    return;
  }

  switch (inst->port->drv_type) {
    case DRIVER_TYPE_SENSOR:
      pm_print(ESP_LOG_INFO, inst->port->desc.s.measurements, "DEBUG");
      break;
    case DRIVER_TYPE_ACTUATOR:
      ESP_LOGI("DEBUG", "[localId=%d] ", inst->port->desc.a.local_id);
      break;
    case DRIVER_TYPE_COMMAND_API:
      ESP_LOGI("DEBUG", "[localId=%d] ", inst->port->desc.c.local_id);
      break;
  }

  ESP_LOGI(
    "DEBUG", "|name=%s| |active=%d| |drv=%s type=%d|", inst->port->port_name, inst->online,
    inst->port->drv_name, inst->port->drv_type
  );
}

// Find instance by localId
static inline sn_device_instance_t *find_instance_by_local_id(uint8_t local_id) {
  FOR_EACH_INSTANCE(inst, gDeviceInstances, gDeviceInstancesLen) {
    switch (inst->port->drv_type) {
      case DRIVER_TYPE_SENSOR: {
        FOR_EACH_MEASUREMENT(it, inst->port->desc.s.measurements) {
          if (it->local_id == local_id) {
            return (sn_device_instance_t *)inst;
          }
        }
        continue;
      }
      case DRIVER_TYPE_ACTUATOR: {
        if (inst->port->desc.a.local_id == local_id) {
          return (sn_device_instance_t *)inst;
        }
        continue;
      }
      case DRIVER_TYPE_COMMAND_API: {
        if (inst->port->desc.c.local_id == local_id) {
          return (sn_device_instance_t *)inst;
        }
        continue;
      }
    }
  }
  return NULL;
}

// Find all instance by type (first match)
static inline sn_device_instance_t *find_instance_by_driver_name(const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < gDeviceInstancesLen; ++i) {
    const sn_device_instance_t *inst = &gDeviceInstances[i];
    if (inst->port && strcmp(inst->port->drv_name, name) == 0) return (sn_device_instance_t *)inst;
  }
  return NULL;
}

// --------------------------------------------------------------------------------
// API
// --------------------------------------------------------------------------------

/*
 * @brief Add driver to internal registry
 */
esp_err_t sn_driver_register(const sn_driver_desc_t *desc);

void sn_driver_bind_all_ports(const sn_device_port_desc_t *ports, size_t port_len);

sn_device_instance_t *sn_driver_get_device_instances();

size_t sn_driver_get_instance_len();

#endif // !SN_DEVICE_DRIVER_H
