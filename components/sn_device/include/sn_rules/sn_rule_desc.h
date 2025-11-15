#ifndef SN_RULE_DESC_H
#define SN_RULE_DESC_H

#include "forward.h"
#include "sn_json.h"

typedef struct {
  // Name of the rule
  const char *name;

  // command to trigger on thresshold
  const sn_command_t *on_low;
  const sn_command_t *on_normal;
  const sn_command_t *on_high;

  // command to trigger on state change
  const sn_command_t *on_exit_low;
  const sn_command_t *on_exit_normal;
  const sn_command_t *on_exit_high;

  // Low and high thresshold
  double high;
  double low;

  // Rule's priority. Higher is prioritized
  int priority;

  // Id of this rules
  local_id_t id;

  // Source id of type local_id_t for identifying
  // the sensors source
  local_id_t src_id;
} sn_rule_desc_t;

#endif // !SN_RULE_DESC_H
