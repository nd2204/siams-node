#ifndef SN_PORT_DESC_H
#define SN_PORT_DESC_H

#include "forward.h"
#include "sn_driver/sensor.h"
#include "soc/gpio_num.h"
#include <stdint.h>

// clang-format off
typedef enum {
  DRIVER_TYPE_SENSOR = 0,
  DRIVER_TYPE_ACTUATOR,
  DRIVER_TYPE_COMMAND_API
} driver_type_e;

typedef enum {
  PUT_GPIO = 0,
  PUT_I2C,
  PUT_SPI
} sn_port_usage_type_e;

typedef union {
  struct { gpio_num_t pin; } gpio;
  struct { int addr; gpio_num_t sda; gpio_num_t scl; } i2c;
  struct { int host; gpio_num_t cs; } spi;
} sn_port_usage_u;
// clang-format on

typedef struct {
  const sn_port_measurement_map_t *measurements; // NULL-terminated array (for sensors)
  sn_port_usage_u usage;
  sn_port_usage_type_e usage_type;
  int measurements_count;
  uint32_t sample_rate_ms;
} sn_sensor_port_t;

typedef struct {
  sn_port_usage_u usage;
  sn_port_usage_type_e usage_type;
  local_id_t local_id;
} sn_actuator_port_t;

typedef struct {
  sn_port_usage_u usage;
  sn_port_usage_type_e usage_type;
  local_id_t local_id;
} sn_command_api_port_t;

typedef struct {
  const char *port_name;  // for identity: "dht-1", "soil-1", "pump_action"
  const char *drv_name;   // driver key: "dht22","soil_analog","relay"
  driver_type_e drv_type; // sensor or actuator
  union {
    sn_actuator_port_t a;
    sn_sensor_port_t s;
    sn_command_api_port_t c;
  } desc;
} sn_device_port_desc_t;

// Helpers macro

#define SENSOR_PORT_LITERAL(PORT_NAME, DRV_NAME, DESC)                                             \
  {.port_name = PORT_NAME, .desc.s = DESC, .drv_type = DRIVER_TYPE_SENSOR, .drv_name = DRV_NAME}

#define ACTUATOR_PORT_LITERAL(PORT_NAME, DRV_NAME, DESC)                                           \
  {.port_name = PORT_NAME, .desc.a = DESC, .drv_type = DRIVER_TYPE_ACTUATOR, .drv_name = DRV_NAME}

#define COMMAND_PORT_LITERAL(PORT_NAME, DRV_NAME, DESC)                                            \
  {.port_name = PORT_NAME, .desc.a = DESC, .drv_type = DRIVER_TYPE_COMMAND, .drv_name = DRV_NAME}

#define DEFINE_SENSOR_PORT_CONST_VAR(VAR_NAME, PORT_NAME, DRV_NAME, DESC)                          \
  const sn_device_port_desc_t VAR_NAME = SENSOR_PORT_LITERAL(PORT_NAME, DRV_NAME, DESC);

#define DEFINE_ACTUATOR_PORT_CONST_VAR(VAR_NAME, PORT_NAME, DRV_NAME, DESC)                        \
  const sn_device_port_desc_t VAR_NAME = ACTUATOR_PORT_LITERAL(PORT_NAME, DRV_NAME, DESC);

#define DEFINE_COMMAND_PORT_CONST_VAR(VAR_NAME, PORT_NAME, DRV_NAME, DESC)                         \
  const sn_device_port_desc_t VAR_NAME = COMMAND_PORT_LITERAL(PORT_NAME, DRV_NAME, DESC);

#endif // !SN_PORT_DESC_H
