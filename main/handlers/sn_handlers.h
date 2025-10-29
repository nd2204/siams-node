#ifndef SN_HANDLERS_H
#define SN_HANDLERS_H

#include "sn_capability.h"

DECLARE_ACTION_HANDLER(led_handler);
DECLARE_ACTION_HANDLER(restart_handler);
DECLARE_ACTION_HANDLER(pump_handler);
DECLARE_ACTION_HANDLER(set_sample_rate_handler);
DECLARE_ACTION_HANDLER(set_moist_pump_threshold);

#endif // !SN_HANDLERS_H
