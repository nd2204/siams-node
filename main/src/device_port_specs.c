#include "sn_driver/port_desc.h"
#include "sn_driver/sensor.h"
#include "sn_json.h"
#include "sn_rules/sn_rule_desc.h"
#include "sn_rules/sn_rule_engine.h"

// How to pick a local_id
// 0x01 -> 0x7E: (126) For sensors, actuators, commands ports
// 0x7F -> 0xFE: (126) For rules

// Define globals (device metadata)

// TODO: Add persistence for sensors and actuator states
static const sn_port_measurement_map_t dht1_temperature_map =
  MEASUREMENT_MAP_ENTRY(0x01, ST_TEMPERATURE, "C");

static const sn_port_measurement_map_t dht1_humidity_map =
  MEASUREMENT_MAP_ENTRY(0x02, ST_HUMIDITY, "%");

static const sn_port_measurement_map_t dht1_map[] = {
  dht1_temperature_map, dht1_humidity_map, MEASUREMENT_MAP_ENTRY_NULL()
};

static const sn_port_measurement_map_t soil1_moisture_map =
  MEASUREMENT_MAP_ENTRY(0x03, ST_MOISTURE, "%");

static const sn_port_measurement_map_t soil1_map[] = {
  soil1_moisture_map,
  MEASUREMENT_MAP_ENTRY_NULL(),
};

static const sn_port_measurement_map_t light1_intensity =
  MEASUREMENT_MAP_ENTRY(0x04, ST_LIGHT_INTENSITY, "lux");

static const sn_port_measurement_map_t light1_map[] = {
  light1_intensity,
  MEASUREMENT_MAP_ENTRY_NULL(),
};

#define SENSOR_PORT_DESCS(X)                                                                       \
  X(soil_1, "soil-1", "soil_adc",                                                                  \
    ((sn_sensor_port_t){.usage.gpio.pin = GPIO_NUM_35,                                             \
                        .usage_type = PUT_GPIO,                                                    \
                        .measurements = soil1_map,                                                 \
                        .sample_rate_ms = 5000}))                                                  \
  X(light_1, "light-1", "lm393",                                                                   \
    ((sn_sensor_port_t){.usage.gpio.pin = GPIO_NUM_34,                                             \
                        .usage_type = PUT_GPIO,                                                    \
                        .measurements = light1_map,                                                \
                        .sample_rate_ms = 10000}))                                                 \
  X(dht_1, "dht-1", "dht11",                                                                       \
    ((sn_sensor_port_t){.usage.gpio.pin = GPIO_NUM_23,                                             \
                        .usage_type = PUT_GPIO,                                                    \
                        .measurements = dht1_map,                                                  \
                        .sample_rate_ms = 8000}))

#define ACTUATOR_PORT_DESCS(X)                                                                     \
  X(led_yellow, "led-yellow", "led",                                                               \
    ((sn_actuator_port_t){.local_id = 0x0A,                                                        \
                          .usage.gpio.pin = GPIO_NUM_18,                                           \
                          .usage_type = PUT_GPIO}))                                                \
  X(led_green, "led-green", "led",                                                                 \
    ((sn_actuator_port_t){.local_id = 0x0B,                                                        \
                          .usage.gpio.pin = GPIO_NUM_19,                                           \
                          .usage_type = PUT_GPIO}))                                                \
  X(                                                                                               \
    led_red, "led-red", "led",                                                                     \
    ((sn_actuator_port_t){.local_id = 0x0C, .usage.gpio.pin = GPIO_NUM_3, .usage_type = PUT_GPIO}) \
  )                                                                                                \
  X(pump_1, "pump-1", "relay",                                                                     \
    ((sn_actuator_port_t){                                                                         \
      .local_id = 0x0D,                                                                            \
      .usage.gpio.pin = GPIO_NUM_5,                                                                \
      .usage_type = PUT_GPIO,                                                                      \
    }))                                                                                            \
  X(oled, "oled-screen", "ssd1306",                                                                \
    ((sn_actuator_port_t){                                                                         \
      .local_id = 0x0E,                                                                            \
      .usage.i2c.scl = GPIO_NUM_22,                                                                \
      .usage.i2c.sda = GPIO_NUM_21,                                                                \
      .usage_type = PUT_I2C,                                                                       \
    }))

#define COMMAND_PORT_DESC(X)                                                                       \
  X(sensor_control, "sensor control", "sensor_control",                                            \
    ((sn_command_api_port_t){                                                                      \
      .local_id = 0x7E,                                                                            \
    }))

SENSOR_PORT_DESCS(DEFINE_SENSOR_PORT_CONST_VAR)
ACTUATOR_PORT_DESCS(DEFINE_ACTUATOR_PORT_CONST_VAR);
COMMAND_PORT_DESC(DEFINE_COMMAND_PORT_CONST_VAR);

const sn_device_port_desc_t gDevicePorts[] = {
#define DEVICE_PORTS_LIST(VAR_NAME, ...) VAR_NAME,
  SENSOR_PORT_DESCS(DEVICE_PORTS_LIST) ACTUATOR_PORT_DESCS(DEVICE_PORTS_LIST)
    COMMAND_PORT_DESC(DEVICE_PORTS_LIST)
#undef DEVICE_PORTS_LIST
};

//--------------------------------------------------------------------------------
// RULES
//--------------------------------------------------------------------------------
const sn_command_t soil_moisture_on_exit_low[] = {
  {.local_id = led_yellow.desc.a.local_id,
   .action = "control_led",
   .params_json = "{\"enable\":false}"},
  COMMAND_NULL_ENTRY
};

const sn_command_t soil_moisture_on_exit_normal[] = {
  {.local_id = led_green.desc.a.local_id,
   .action = "control_led",
   .params_json = "{\"enable\":false}"},
  COMMAND_NULL_ENTRY
};

const sn_command_t soil_moisture_on_exit_high[] = {
  {.local_id = led_red.desc.a.local_id,
   .action = "control_led",
   .params_json = "{\"enable\":false}"},
  COMMAND_NULL_ENTRY
};

const sn_command_t soil_moisture_on_low_commands[] = {
  {.local_id = led_yellow.desc.a.local_id,
   .action = "control_led",
   .params_json = "{\"enable\":true, \"brightness\": 1}"},
  {.local_id = oled.desc.a.local_id,
   .action = "set_message",
   .params_json = "{\"msg\":\"Moisture Low\"}"          },
  {.local_id = pump_1.desc.a.local_id,
   .action = "control_relay",
   .params_json = "{\"enable\":true}"                   },
  COMMAND_NULL_ENTRY
};

const sn_command_t soil_moisture_on_normal_commands[] = {
  {.local_id = led_green.desc.a.local_id,
   .action = "control_led",
   .params_json = "{\"enable\":true, \"brightness\": 1}"},
  {.local_id = oled.desc.a.local_id,
   .action = "set_message",
   .params_json = "{\"msg\":\"Moisture Normal\"}"       },
  {.local_id = pump_1.desc.a.local_id,
   .action = "control_relay",
   .params_json = "{\"enable\":false}"                  },
  COMMAND_NULL_ENTRY
};

const sn_command_t soil_moisture_on_high_commands[] = {
  {.local_id = led_red.desc.a.local_id,
   .action = "control_led",
   .params_json = "{\"enable\":true, \"brightness\": 1}"},
  {.local_id = oled.desc.a.local_id,
   .action = "set_message",
   .params_json = "{\"msg\":\"Moisture High\"}"         },
  {.local_id = pump_1.desc.a.local_id,
   .action = "control_relay",
   .params_json = "{\"enable\":false}"                  },
  COMMAND_NULL_ENTRY
};

// Rules to act on each telemetry data
const sn_rule_desc_t gRules[] = {
  {
   .id = 0xfe,
   .src_id = soil1_moisture_map.local_id,
   .on_high = soil_moisture_on_high_commands,
   .on_normal = soil_moisture_on_normal_commands,
   .on_low = soil_moisture_on_low_commands,
   .on_exit_high = soil_moisture_on_exit_high,
   .on_exit_normal = soil_moisture_on_exit_normal,
   .on_exit_low = soil_moisture_on_exit_low,
   .name = "soil moisture rule",
   .high = 80,
   .low = 35,
   },
};

const size_t gRulesLen = sizeof(gRules) / sizeof(sn_rule_desc_t);
const size_t gDevicePortsLen = sizeof(gDevicePorts) / sizeof(sn_device_port_desc_t);
// clang-format on
