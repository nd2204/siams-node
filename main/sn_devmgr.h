#ifndef SN_DEV_MGR_H
#define SN_DEV_MGR_H

#include "driver/i2c_master.h"

void sn_devmgr_init();

void sn_devmgr_i2c_master_bus_init();

i2c_master_bus_handle_t sn_devmgr_get_i2c_master_bus_handle();

void sn_devmgr_register_device(const char *name);

#endif // !SN_DEV_MGR_H
