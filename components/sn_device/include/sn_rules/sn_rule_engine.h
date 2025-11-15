#ifndef SN_RULE_ENGINE_H
#define SN_RULE_ENGINE_H

#include "sn_rules/sn_rule_desc.h"
#include <stddef.h>

extern const sn_rule_desc_t gRules[];
extern const size_t gRulesLen;

void rule_engine_task(void *pvParams);

#endif // !SN_RULE_ENGINE_H
