#include "sn_driver/port_desc.h"
#include "sn_driver/sensor.h"

// clang-format off
// Define globals (device metadata)
static const sn_port_measurement_map_t dht1_map[] = {
  MEASUREMENT_MAP_ENTRY(0x01, ST_TEMPERATURE, "C"),
  MEASUREMENT_MAP_ENTRY(0x02, ST_HUMIDITY, "%"),
  MEASUREMENT_MAP_ENTRY_NULL()
};

static const sn_port_measurement_map_t soil1_map[] = {
  MEASUREMENT_MAP_ENTRY(0x03, ST_MOISTURE, "%"),
  MEASUREMENT_MAP_ENTRY_NULL(),
};

static const sn_port_measurement_map_t light1_map[] = {
  MEASUREMENT_MAP_ENTRY(0x04, ST_LIGHT_INTENSITY, "lux"),
  MEASUREMENT_MAP_ENTRY_NULL(),
};

const sn_device_port_desc_t gDevicePorts[] = {
  // sensors (local_id defined in measurements map)
  SENSOR_PORT_LITERAL("soil-1", "soil_adc", ((sn_sensor_port_t) {
    .usage.gpio.pin = GPIO_NUM_34,
    .usage_type = PUT_GPIO,
    .measurements = soil1_map
  })),
    SENSOR_PORT_LITERAL("light-1", "lm393", ((sn_sensor_port_t){
      .usage.gpio.pin = GPIO_NUM_34,
      .usage_type = PUT_GPIO,
      .measurements = light1_map
    })),
  SENSOR_PORT_LITERAL("dht-1", "dht11", ((sn_sensor_port_t) {
    .usage.gpio.pin = GPIO_NUM_23,
    .usage_type = PUT_GPIO,
    .measurements = dht1_map
  })),
  // actuator
  ACTUATOR_PORT_LITERAL("led-yellow", "led", ((sn_actuator_port_t){
    .local_id = 0x0a,
    .usage.gpio.pin = GPIO_NUM_18,
    .usage_type = PUT_GPIO
  })),
  ACTUATOR_PORT_LITERAL("led-green", "led", ((sn_actuator_port_t){
    .local_id = 0x0b,
    .usage.gpio.pin = GPIO_NUM_19,
    .usage_type = PUT_GPIO
  })),
  ACTUATOR_PORT_LITERAL("pump-1", "relay", ((sn_actuator_port_t){
    .local_id = 0x0c,
    .usage.gpio.pin = GPIO_NUM_5,
    .usage_type = PUT_GPIO,
  })),
};

const size_t gDevicePortsLen = sizeof(gDevicePorts) / sizeof(sn_device_port_desc_t);
// clang-format on
