#ifndef SN_SECURITY_H
#define SN_SECURITY_H

#include <stddef.h>
#include "cJSON.h"

cJSON *sn_security_sign_and_wrap_payload(cJSON *payload);

// clang-format off
void sn_security_calculate_hmac(
  const unsigned char *key, size_t key_len,
  const unsigned char *message, size_t msg_len,
  unsigned char *hmac_result
);

void sn_security_byte_to_hex_string(
  const unsigned char *bytes, size_t len, char hex_str[65]
);

// clang-format on

#endif // !SN_SECURITY_H
