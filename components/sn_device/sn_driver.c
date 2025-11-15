#include "sn_driver.h"
#include "esp_log.h"
#include "sn_adc_helper.h"
#include "sn_driver/driver_inst.h"

#define MAX_DRIVERS 8

static const char *TAG = "SN_DRIVER";
static const sn_driver_desc_t *driver_registry[MAX_DRIVERS];
static int driver_registry_len = 0;
sn_device_instance_t gDeviceInstances[MAX_INSTANCES];
size_t gDeviceInstancesLen = 0;

/* bus mutexes (for i2c/spi shared protection if needed) */
// static SemaphoreHandle_t i2c_mutex = NULL;
// static SemaphoreHandle_t spi_mutex = NULL;

/* Helper: find driver by supported_driver_name */
static bool supports_driver(const sn_driver_desc_t *d, const char *name) {
  if (!d || !name || !d->supported_types) return false;
  for (const char **p = d->supported_types; *p; ++p) {
    if (strcmp(*p, name) == 0) return true;
  }
  return false;
}

static inline const char *get_driver_type_str(driver_type_e type) {
#define STR(x) #x
  switch (type) {
    case DRIVER_TYPE_SENSOR:
      return STR(DRIVER_TYPE_SENSOR);
    case DRIVER_TYPE_ACTUATOR:
      return STR(DRIVER_TYPE_ACTUATOR);
    case DRIVER_TYPE_COMMAND_API:
      return STR(DRIVER_TYPE_COMMAND_API);
  }
#undef STR
  return "DRIVER_TYPE_UNKNOWN";
}

esp_err_t sn_driver_register(const sn_driver_desc_t *desc) {
  if (!desc) return ESP_ERR_INVALID_ARG;
  if (driver_registry_len >= MAX_DRIVERS) return ESP_ERR_NO_MEM;
  driver_registry[driver_registry_len++] = desc;
  ESP_LOGI(TAG, "Driver registered: %s (priority=%d)", desc->name, desc->priority);
  return ESP_OK;
}

void sn_driver_bind_all_ports(const sn_device_port_desc_t *ports, size_t ports_len) {
  adc_helper_init();
  gDeviceInstancesLen = 0;

  for (size_t i = 0; i < ports_len && gDeviceInstancesLen < MAX_INSTANCES; ++i) {
    const sn_device_port_desc_t *p = &ports[i];
    const sn_driver_desc_t *best_drv = NULL;
    int best_prio = -99999;
    // Go through the registry and detect the highest priority driver
    for (int j = 0; j < driver_registry_len; ++j) {
      const sn_driver_desc_t *d = driver_registry[j];
      if (!supports_driver(d, p->drv_name)) continue;
      // protective locking for i2c/spi if needed would occur here
      bool ok = d->probe ? d->probe(p) : false;
      if (ok && d->priority > best_prio) {
        best_drv = d;
        best_prio = d->priority;
      }
    }

    // create and assign ports to device instance
    sn_device_instance_t *inst = &gDeviceInstances[gDeviceInstancesLen];
    memset(inst, 0, sizeof(sn_device_instance_t));
    inst->port = p;
    inst->driver = best_drv;
    inst->interval_ms = 2000;
    inst->last_read_ms = 0;
    inst->consecutive_failures = 0;
    inst->bus_lock = NULL;

    const char *tstr = get_driver_type_str(p->drv_type);

    if (!best_drv) {
      inst->online = false;
      ESP_LOGE(TAG, "No driver for %s '%s' drv_name='%s'", tstr, p->port_name, p->drv_name);
    } else {
      esp_err_t err = best_drv->init ? best_drv->init(p, &inst->ctx, sizeof(inst->ctx)) : ESP_OK;
      if (err == ESP_OK) {
        inst->online = true;
        // optional printing for driver type
        switch (inst->port->drv_type) {
          case DRIVER_TYPE_SENSOR: {
            inst->interval_ms = p->desc.s.sample_rate_ms;
            ESP_LOGW(
              TAG, "[%02d]: Bound %s '%s' -> driver '%s' sample_rate=%d", gDeviceInstancesLen, tstr,
              p->port_name, best_drv->name, inst->interval_ms
            );
            pm_print(ESP_LOG_WARN, p->desc.s.measurements, TAG);
          } break;
          case DRIVER_TYPE_ACTUATOR: {
            ESP_LOGW(
              TAG, "[%02d][localId=%d]: Bound %s '%s' -> driver '%s'", gDeviceInstancesLen,
              p->desc.a.local_id, tstr, p->port_name, best_drv->name
            );
          } break;
          case DRIVER_TYPE_COMMAND_API: {
            ESP_LOGW(
              TAG, "[%02d][localId=%d]: Bound %s '%s' -> driver '%s'", gDeviceInstancesLen,
              p->desc.c.local_id, tstr, p->port_name, best_drv->name
            );
          } break;
        }
      } else {
        inst->online = false;
        ESP_LOGW(
          TAG, "Driver '%s' init failed for %s '%s' err=%d", best_drv->name, tstr, p->port_name, err
        );
        if (best_drv->deinit) best_drv->deinit(&inst->ctx);
      }
    }
    gDeviceInstancesLen++;
  }

  ESP_LOGW(TAG, "Bound %d devices", gDeviceInstancesLen);
}

sn_device_instance_t *sn_driver_get_device_instances() { return gDeviceInstances; }

size_t sn_driver_get_instance_len() { return gDeviceInstancesLen; }
