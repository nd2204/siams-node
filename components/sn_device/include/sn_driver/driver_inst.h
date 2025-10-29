#ifndef SN_DRIVER_INSTANCE_H
#define SN_DRIVER_INSTANCE_H

#include "sn_driver/driver_desc.h"
#include "sn_driver/port_desc.h"

#include "freertos/idf_additions.h"
#include <stdint.h>

typedef enum { CTX_NONE = 0, CTX_GENERIC } ctx_tag_e;

/* For simplicity we store ctx inline as a small byte array in instance. */
typedef union {
  uint8_t raw[64];
} sn_driver_ctx_u;

/*
 * @brief runtime device port metadata
 * this struct is used for managing driver on a port as well as port/pin status
 */

// clang-format off
typedef struct {
  sn_driver_ctx_u              ctx;
  const sn_device_port_desc_t *port;
  const sn_driver_desc_t      *driver;
  SemaphoreHandle_t            bus_lock; // optional per-instance bus lock pointer if needed
  uint64_t                     last_read_ms;
  uint32_t                     interval_ms;
  ctx_tag_e                    ctx_tag;
  int                          consecutive_failures;
  bool                         online;
} sn_device_instance_t;
//clang-format on

#endif // !SN_DRIVER_INSTANCE_H
