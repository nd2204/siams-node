#ifndef SN_DEVICE_H
#define SN_DEVICE_H

#include "cJSON.h"
#include "soc/gpio_num.h"
#include <stdint.h>

typedef int (*action_handler_t)(const cJSON *params, cJSON **out_result);

#define DECLARE_ACTION_HANDLER(FNNAME) int FNNAME(const cJSON *params, cJSON **out_result);

typedef uint8_t local_id_t;
#define LOCAL_ID_MAX 0xff
#define LOCAL_ID_MIN 0x01

/*
 * Sensors capability
 */
typedef struct {
  local_id_t localId;
  const char *type;
  char unit[8];
  uint32_t sampleRateMs;
} sensor_capability_t;

/*
 * Actuator capability
 */
typedef struct {
  const char *type;
  local_id_t localId;
  gpio_num_t pin;
} actuator_capability_t;

/*
 * Command capability
 */
typedef struct {
  const char *action;
  const char **params;
  int params_len;
  local_id_t localId;
} command_capability_t;

/*
 * Device capability (aggregation of other capabilities)
 */
typedef struct {
  const sensor_capability_t *sensors;
  const actuator_capability_t *actuators;
  const command_capability_t *commands;
  size_t slen, alen, clen;
} device_capabilities_t;

/*
 * Command entry for mapping
 */
typedef struct {
  local_id_t localId;
  action_handler_t handler;
} command_handler_entry_t;

// Khai báo extern để có thể include từ nhiều file
extern const sensor_capability_t gSensorCapabilities[];
extern const size_t SENSOR_COUNT;

extern const actuator_capability_t gActuatorCapabilities[];
extern const size_t ACTUATOR_COUNT;

extern const command_capability_t gCommandCapabilities[];
extern const size_t COMMAND_COUNT;

extern const command_handler_entry_t gCommandHandlers[];
extern const size_t HANDLER_COUNT;

// ----- lookup helpers -----
const sensor_capability_t *find_sensor_meta(local_id_t sensor_id);

const actuator_capability_t *find_actuator_meta(local_id_t actuator_id);

const command_capability_t *find_command_meta(const char *action);

const command_handler_entry_t *find_command_handler(local_id_t cmd_id);

cJSON *device_capabilities_to_json(const device_capabilities_t *const c);

int dispatch_command(const char *payload_json, cJSON **out_result);

#endif // !SN_DEVICE_H
