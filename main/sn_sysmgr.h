#ifndef SN_SYSMGR_H
#define SN_SYSMGR_H

#include "esp_err.h"
#include <stdbool.h>

typedef esp_err_t (*sys_cb_t)(void);

typedef struct {
  sys_cb_t init_cb;
  sys_cb_t shutdown_cb;
  const char *name;
} sys_config_t;

void sn_sysmgr_init_self();

void sn_sysmgr_init_all();

void sn_sysmgr_register(sys_config_t *system_cb);

void sn_sysmgr_shutdown_all();

bool sn_sysmgr_is_valid();

#define SN_SYSMGR_REGISTER(_name, _init_cb, _shutdown_cb)                                          \
  do {                                                                                             \
    sys_config_t config = {.init_cb = _init_cb, .shutdown_cb = _shutdown_cb, .name = _name};       \
    sn_sysmgr_register(&config);                                                                   \
  } while (0)

#endif // !SN_SYSMGR_H
