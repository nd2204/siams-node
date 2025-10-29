#ifndef SN_DRIVER_DESC_H
#define SN_DRIVER_DESC_H

#include "sn_driver/port_desc.h"
#include "sn_driver/sensor.h"
#include "sn_driver/actuator.h"

#include "esp_err.h"
#include "sn_json.h"
#include <stdbool.h>

typedef struct {
  const char *name;
  const char **supported_types; // NULL-terminated
  const sn_command_desc_t *command_desc;
  int priority;
  bool (*probe)(const sn_device_port_desc_t *port);
  esp_err_t (*init)(const sn_device_port_desc_t *port, void *ctx_out, size_t ctx_size);
  void (*deinit)(void *ctx);
  read_multi_fn_t read_multi; // NULL if actuator-only
  control_fn_t control;       // NULL if sensor-only
} sn_driver_desc_t;

#endif // !SN_DRIVER_DESC_H
