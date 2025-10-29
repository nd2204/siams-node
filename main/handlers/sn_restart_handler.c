#include "esp_system.h"
#include "sn_handlers.h"

DECLARE_ACTION_HANDLER(restart_handler) {
  esp_restart();
  return 0;
}
