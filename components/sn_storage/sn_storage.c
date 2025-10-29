#include "sn_storage.h"
#include "sn_error.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "SN_STORAGE";

#define DEFINE_GETTER_SETTER(KEY)                                                                  \
  esp_err_t sn_storage_set_##KEY(const char *KEY) { return sn_storage_set_string(#KEY, KEY); }     \
  esp_err_t sn_storage_get_##KEY(char *out, size_t len) {                                          \
    return sn_storage_get_string(#KEY, out, len);                                                  \
  }

// --------------------------------------------------------------------------------
// Init & teardown
// --------------------------------------------------------------------------------

esp_err_t sn_storage_init(void *params) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "Erasing NVS due to version mismatch or full pages...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "NVS initialized");
  return ESP_OK;
}

esp_err_t sn_storage_clear() { return nvs_flash_erase(); }

// --------------------------------------------------------------------------------
// Generic helpers
// --------------------------------------------------------------------------------

esp_err_t sn_storage_set_string(const char *key, const char *value) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs);
  if (err != ESP_OK) return err;

  err = nvs_set_str(nvs, key, value);
  if (err == ESP_OK) err = nvs_commit(nvs);
  nvs_close(nvs);
  return err;
}

esp_err_t sn_storage_get_string(const char *key, char *out, size_t len) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs);
  if (err != ESP_OK) return err;

  size_t required = len;
  err = nvs_get_str(nvs, key, out, &required);
  nvs_close(nvs);
  return err;
}

esp_err_t sn_storage_erase_key(const char *key) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs);
  if (err != ESP_OK) return err;

  err = nvs_erase_key(nvs, key);
  if (err == ESP_OK) err = nvs_commit(nvs);
  nvs_close(nvs);
  return err;
}

// --------------------------------------------------------------------------------
// Device Info
// --------------------------------------------------------------------------------

DEFINE_GETTER_SETTER(device_id)
DEFINE_GETTER_SETTER(device_capabilities)
DEFINE_GETTER_SETTER(org_id)
DEFINE_GETTER_SETTER(cluster_id)

// --------------------------------------------------------------------------------
// Credentials
// --------------------------------------------------------------------------------

esp_err_t sn_storage_set_credentials(const char *username, const char *password) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs);
  if (err != ESP_OK) return err;

  GOTO_IF_FALSE(cleanup, (err = nvs_set_str(nvs, "mqtt_user", username)) != ESP_OK);
  GOTO_IF_FALSE(cleanup, (err = nvs_set_str(nvs, "mqtt_pass", password)) != ESP_OK);
  err = nvs_commit(nvs);

cleanup:
  nvs_close(nvs);
  return err;
}

esp_err_t sn_storage_get_credentials(char *username, size_t ulen, char *password, size_t plen) {
  nvs_handle_t nvs;
  esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs);
  if (err != ESP_OK) return err;
  GOTO_IF_FALSE(cleanup, (err = nvs_get_str(nvs, "mqtt_user", username, &ulen)) != ESP_OK);
  GOTO_IF_FALSE(cleanup, (err = nvs_get_str(nvs, "mqtt_pass", password, &plen)) != ESP_OK);
  err = nvs_commit(nvs);

cleanup:
  nvs_close(nvs);
  return err;
}

// --------------------------------------------------------------------------------
// Debug utilities
// --------------------------------------------------------------------------------

void sn_storage_list_all(void) {
  nvs_iterator_t it = NULL;
  esp_err_t res = nvs_entry_find("nvs", STORAGE_NAMESPACE, NVS_TYPE_ANY, &it);
  ESP_LOGI(TAG, "Listing NVS entries:");
  nvs_entry_info_t info;
  while (res == ESP_OK) {
    nvs_entry_info(it, &info);
    ESP_LOGI(TAG, "  key: %-15s | type: %d", info.key, info.type);
    res = nvs_entry_next(&it);
  }

  nvs_release_iterator(it);
}
