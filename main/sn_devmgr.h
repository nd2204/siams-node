#ifndef SN_DEV_MGR_H
#define SN_DEV_MGR_H

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"

esp_err_t sn_devmgr_init();

void sn_devmgr_i2c_scan_all(bool printResult);

esp_err_t sn_devmgr_i2c_scan_address(uint8_t addr);

i2c_master_bus_handle_t sn_devmgr_get_i2c_master_bus_handle();

void sn_devmgr_register_device(const char *name);

SemaphoreHandle_t sn_devmgr_get_i2c_master_bus_mutex();

#endif // !SN_DEV_MGR_H
