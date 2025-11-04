#ifndef SN_ACTUATOR_DRIVER_H
#define SN_ACTUATOR_DRIVER_H

#include "cJSON.h"
#include "esp_err.h"

#define ACTUATOR_TYPE(X)                                                                           \
  X(LED)                                                                                           \
  X(PUMP)

typedef enum {
#define TO_ENUM_FIELD(c) AT_##c,
  ACTUATOR_TYPE(TO_ENUM_FIELD)
#undef TO_ENUM_FIELD
} actuator_type_e;

// control: used for actuators. params provided as simple name/value (cJSON could be used)
typedef esp_err_t (*control_fn_t)(void *ctx, const cJSON *paramsJson, cJSON **resultJsonOut);

#endif // !SN_ACTUATOR_DRIVER_H
