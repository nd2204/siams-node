#ifndef SN_CAPABILITY_H
#define SN_CAPABILITY_H

#include "cJSON.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "sn_driver/port_desc.h"
#include "sn_json.h"
#include <stdint.h>

#define LOCAL_ID_MAX 0xff
#define LOCAL_ID_MIN 0x01

#define DECLARE_ACTION_HANDLER(FNNAME) int FNNAME(const cJSON *params, cJSON **out_result)

typedef int (*action_handler_t)(const cJSON *params, cJSON **out_result);

typedef struct {
  const char *action;
  const char **params;
  action_handler_t handler;
} command_capability_t;

static inline const char *get_chip_model_str(esp_chip_model_t model) {
  // clang-format off
  switch (model) {
    case CHIP_ESP32:       return "ESP32";
    case CHIP_ESP32S2:     return "ESP32S2";
    case CHIP_ESP32S3:     return "ESP32S3";
    case CHIP_ESP32C3:     return "ESP32C3";
    case CHIP_ESP32C2:     return "ESP32C2";
    case CHIP_ESP32C6:     return "ESP32C6";
    case CHIP_ESP32H2:     return "ESP32H2";
    case CHIP_ESP32P4:     return "ESP32P4";
    case CHIP_ESP32C61:    return "ESP32C61";
    case CHIP_ESP32C5:     return "ESP32C5";
    case CHIP_ESP32H21:    return "ESP32H21";
    case CHIP_ESP32H4:     return "ESP32H4";
    case CHIP_POSIX_LINUX: return "POSIX_LINUX";
  }
  return "Unknown";
  // clang-format on
}

static inline esp_err_t get_hwid(char *out, size_t len) {
  if (!out) return ESP_ERR_INVALID_ARG;

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  const char *model = get_chip_model_str(chip_info.model);

  uint8_t mac[6]; // Array to store the 6-byte MAC address
  esp_err_t ok = esp_efuse_mac_get_default(mac);
  snprintf(
    out, len, "%s:%02x:%02x:%02x:%02x:%02x:%02x", model, mac[0], mac[1], mac[2], mac[3], mac[4],
    mac[5]
  );
  return ok;
}

cJSON *device_ports_to_capabilities_json();

int dispatch_command(const char *payload_json, cJSON **out_result);

int sn_dispatch_command_struct(const sn_command_t *command, cJSON **out_result);

#endif // !SN_CAPABILITY_H
