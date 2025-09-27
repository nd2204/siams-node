#include "sn_devmgr.h"
#include "sn_config.h"

#include <driver/i2c_master.h>
#include <driver/i2c_types.h>
#include <esp_err.h>
#include <esp_log.h>

static const char *TAG = "sn-devmgr";

i2c_master_bus_handle_t g_masterBus = NULL;

void sn_devmgr_init() { sn_devmgr_i2c_master_bus_init(); }

void sn_devmgr_i2c_master_bus_init() {
  i2c_master_bus_config_t bus_config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .i2c_port = I2C_BUS_PORT,
      .sda_io_num = SDA_PIN_NUM,
      .scl_io_num = SCL_PIN_NUM,
      .flags.enable_internal_pullup = true,
  };

  ESP_LOGI(TAG, "Initialize I2C bus");
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &g_masterBus));
}

i2c_master_bus_handle_t sn_devmgr_get_i2c_master_bus_handle() {
  assert(g_masterBus != NULL);
  return g_masterBus;
}

void sn_devmgr_register_device(const char *name) {}
