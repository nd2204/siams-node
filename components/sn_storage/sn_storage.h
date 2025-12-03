#ifndef SN_STORAGE_H
#define SN_STORAGE_H

#include "esp_err.h"
#define STORAGE_NAMESPACE "sn_storage"

#define DECLARE_GETTER_SETTER_DELETE(KEY)                                                          \
  esp_err_t sn_storage_set_##KEY(const char *KEY);                                                 \
  esp_err_t sn_storage_get_##KEY(char *out, size_t len);                                           \
  esp_err_t sn_storage_erase_##KEY();

// --------------------------------------------------------------------------------
// Init & teardown
// --------------------------------------------------------------------------------

/*
 *
 */
esp_err_t sn_storage_init(void *params);

/*
 *
 */
esp_err_t sn_storage_clear();

// --------------------------------------------------------------------------------
// Generic helpers
// --------------------------------------------------------------------------------

/*
 *
 */
esp_err_t sn_storage_set_string(const char *key, const char *value);

/*
 *
 */
esp_err_t sn_storage_get_string(const char *key, char *out, size_t len);

/*
 *
 */
esp_err_t sn_storage_erase_key(const char *key);

// --------------------------------------------------------------------------------
// Debug utilities
// --------------------------------------------------------------------------------

/*
 *
 */
void sn_storage_list_all(void);

// --------------------------------------------------------------------------------
// Device Info
// --------------------------------------------------------------------------------
DECLARE_GETTER_SETTER_DELETE(device_id);
DECLARE_GETTER_SETTER_DELETE(device_secret);
DECLARE_GETTER_SETTER_DELETE(device_capabilities);
DECLARE_GETTER_SETTER_DELETE(org_id);
DECLARE_GETTER_SETTER_DELETE(cluster_id);
// --------------------------------------------------------------------------------
// Credentials
// --------------------------------------------------------------------------------

esp_err_t sn_storage_set_credentials(const char *username, const char *password);

esp_err_t sn_storage_get_credentials(char *username, size_t ulen, char *password, size_t plen);

#undef DECLARE_GETTER_SETTER

#endif // !SN_STORAGE_H
