#include "sn_security.h"
#include "cJSON.h"
#include "esp_err.h"
#include "mbedtls/md.h"
#include "sn_storage.h"
#include "sn_sntp.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// utilities function for wrapping the payload
cJSON *sn_security_sign_and_wrap_payload(cJSON *payload) {
  static unsigned char s_device_secret[65] = {0};
  static size_t s_secret_len = 0;
  static bool s_has_secret = false;

  if (!payload) return NULL;

  // create wrapper
  cJSON *wrapper = cJSON_CreateObject();
  char *payload_str = cJSON_PrintUnformatted(payload);
  cJSON_AddItemToObject(wrapper, "raw_payload", cJSON_CreateString(payload_str));
  cJSON_AddNumberToObject(wrapper, "ts", sn_get_unix_timestamp_ms());

  if (!s_has_secret) {
    // try fetching secret from nvs
    esp_err_t ok = sn_storage_get_device_secret((char *)s_device_secret, sizeof(s_device_secret));
    if (ok != ESP_OK) {
      // if device doesn't have secret then skipping signature
      return wrapper;
    } else {
      s_secret_len = strlen((char *)s_device_secret);
      s_has_secret = true;
    }
  }

  unsigned char hmac_result[32] = {0};
  char hmac_hex[65];
  sn_security_calculate_hmac(
    s_device_secret, s_secret_len, (unsigned char *)payload_str, strlen(payload_str), hmac_result
  );
  sn_security_byte_to_hex_string(hmac_result, sizeof(hmac_result), hmac_hex);
  cJSON_AddItemToObject(wrapper, "sig", cJSON_CreateString(hmac_hex));

  // free dangle resource
  cJSON_free(payload_str);
  cJSON_Delete(payload);

  return wrapper;
}

void sn_security_calculate_hmac(
  const unsigned char *key, size_t key_len, const unsigned char *message, size_t msg_len,
  unsigned char *hmac_result
) {
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256; // Using SHA-256

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1); // '1' indicates HMAC
  mbedtls_md_hmac_starts(&ctx, (unsigned char *)key, key_len);
  mbedtls_md_hmac_update(&ctx, (unsigned char *)message, msg_len);
  mbedtls_md_hmac_finish(&ctx, (unsigned char *)hmac_result);
  mbedtls_md_free(&ctx);
}

void sn_security_byte_to_hex_string(const unsigned char *bytes, size_t len, char hex_str[65]) {
  for (size_t i = 0; i < len; i++) {
    // snprintf writes two characters (e.g., "A5") into the string buffer
    snprintf(&hex_str[i * 2], 3, "%02x", bytes[i]);
  }
  // Ensure null termination (handled implicitly by loop boundaries and snprintf behavior)
  hex_str[len * 2] = '\0';
}
