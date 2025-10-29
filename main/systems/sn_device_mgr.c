#include "systems/sn_device_mgr.h"
#include "freertos/FreeRTOS.h"
#include "sn_config.h"

#include <nvs_flash.h>
#include <driver/i2c_master.h>
#include <driver/i2c_types.h>
#include <esp_err.h>
#include <esp_log.h>
#include <string.h>

#define I2C_MAX_ADDRESS   0x78
#define I2C_MIN_ADDRESS   0x03
#define I2C_ADDRESS_RANGE (I2C_MAX_ADDRESS - I2C_MIN_ADDRESS)

static const char *TAG = "sn-devmgr";
static i2c_master_bus_handle_t g_masterBus = NULL;
static SemaphoreHandle_t g_busMutex = NULL;
static esp_err_t g_scanResult[I2C_ADDRESS_RANGE];
static bool g_scanned = false;

esp_err_t sn_devmgr_init(void *args) {
  g_busMutex = xSemaphoreCreateMutex();
  configASSERT(g_busMutex);

  esp_err_t ret = ESP_FAIL;

  ESP_LOGI(TAG, "Initializing i2c master bus");
  i2c_master_bus_config_t bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .i2c_port = I2C_BUS_PORT,
    .sda_io_num = SDA_PIN_NUM,
    .scl_io_num = SCL_PIN_NUM,
    .flags.enable_internal_pullup = true,
  };

  ret = i2c_new_master_bus(&bus_config, &g_masterBus);
  if (ret != ESP_OK) return ret;

  // Initialize NVS
  ESP_LOGI(TAG, "Initializing nvs");
  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  if (ret != ESP_OK) return ret;

  return ret;
}

void sn_devmgr_i2c_scan_all(bool print_stdout) {
  memset(g_scanResult, 0, sizeof(esp_err_t) * I2C_ADDRESS_RANGE);
  ESP_LOGI(TAG, "Starting I2C scan...");

  if (print_stdout) {
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    printf("00:         ");
  }

  // probe 0x03..0x77 inclusive (skip reserved addresses)
  i2c_master_bus_handle_t i2c_bus_handle = sn_devmgr_get_i2c_master_bus_handle();
  for (uint16_t addr = I2C_MIN_ADDRESS; addr < I2C_MAX_ADDRESS; ++addr) {
    // probe the address with a short timeout (ms)
    esp_err_t res = i2c_master_probe(i2c_bus_handle, addr, 500);
    g_scanResult[addr - I2C_MIN_ADDRESS] = res;

    if (print_stdout) {
      if ((addr & 0x0F) == 0) {
        printf("\n%.2x:", addr & 0xF0);
      }
      if (res == ESP_OK) {
        printf(" %.2x", addr);
      } else if (res == ESP_ERR_NOT_FOUND) {
        printf(" --");
      } else {         // timeout or other error
        printf(" ??"); // indicates bus problem / timeout
        ESP_LOGW(TAG, "probe 0x%02x -> %s", addr, esp_err_to_name(res));
      }
    }

    // yield a bit so other tasks can run
    taskYIELD();
  }

  if (print_stdout) {
    printf("\nscan done\n");
  }
  g_scanned = true;
}

void sn_devmgr_i2c_scan(bool print_stdout) {
  vTaskDelay(pdMS_TO_TICKS(200)); // give bus a moment

  memset(g_scanResult, 0, sizeof(esp_err_t) * I2C_ADDRESS_RANGE);
  ESP_LOGI(TAG, "Starting I2C scan...");

  if (print_stdout) {
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    printf("00:         ");
  }

  // probe 0x03..0x77 inclusive (skip reserved addresses)
  i2c_master_bus_handle_t i2c_bus_handle = sn_devmgr_get_i2c_master_bus_handle();
  for (uint16_t addr = I2C_MIN_ADDRESS; addr < I2C_MAX_ADDRESS; ++addr) {
    // probe the address with a short timeout (ms)
    esp_err_t res = i2c_master_probe(i2c_bus_handle, addr, 100);
    g_scanResult[addr - I2C_MIN_ADDRESS] = res;

    if (print_stdout) {
      if ((addr & 0x0F) == 0) {
        printf("\n%.2x:", addr & 0xF0);
      }
      if (res == ESP_OK) {
        printf(" %.2x", addr);
      } else if (res == ESP_ERR_NOT_FOUND) {
        printf(" --");
      } else {         // timeout or other error
        printf(" ??"); // indicates bus problem / timeout
        ESP_LOGW(TAG, "probe 0x%02x -> %s", addr, esp_err_to_name(res));
      }
    }

    // yield a bit so other tasks can run
    taskYIELD();
  }

  if (print_stdout) {
    printf("\nscan done\n");
  }
  g_scanned = true;
}

esp_err_t sn_devmgr_i2c_scan_address(uint8_t addr) {
  if (!g_scanned) {
    sn_devmgr_i2c_scan_all(false);
  }
  return g_scanResult[addr - I2C_MIN_ADDRESS];
}

i2c_master_bus_handle_t sn_devmgr_get_i2c_master_bus_handle() {
  assert(g_masterBus != NULL);
  return g_masterBus;
}

void sn_devmgr_register_device(const char *name) {}

SemaphoreHandle_t sn_devmgr_get_i2c_master_bus_mutex() {
  assert(g_busMutex != NULL);
  return g_busMutex;
}
