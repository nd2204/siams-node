#ifndef SN_DEV_MGR_H
#define SN_DEV_MGR_H

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "soc/gpio_num.h"

typedef enum {
  DEVICE_NONE,
  DEVICE_SOIL_MOISTURE,
  DEVICE_DHTxx,
  DEVICE_LIGHT_SENSOR,
  DEVICE_RELAY,
  DEVICE_PUMP
} DeviceType;

typedef struct {
  int id;
  DeviceType type;
  const char *name;
  gpio_num_t pin;
} HardwarePort;

typedef struct {
  bool connected;
  float last_value;
  uint64_t last_change_ms;
} PortState;

typedef struct {
  bool connected;
  bool active;
} ActuatorState;

esp_err_t sn_devmgr_init(void *args);

void sn_devmgr_i2c_scan_all(bool printResult);

esp_err_t sn_devmgr_i2c_scan_address(uint8_t addr);

i2c_master_bus_handle_t sn_devmgr_get_i2c_master_bus_handle();

void sn_devmgr_register_device(const char *name);

SemaphoreHandle_t sn_devmgr_get_i2c_master_bus_mutex();

#endif // !SN_DEV_MGR_H
