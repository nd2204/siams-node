#include "sn_rules/sn_rule_engine.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "portmacro.h"
#include "sn_capability.h"
#include "sn_driver/sensor.h"
#include "sn_json.h"
#include "sn_rules/sn_rule_desc.h"
#include "sn_telemetry_queue.h"

#define MAX_RULE 126
static const char *TAG = "SN_RULE_ENGINE";

typedef enum {
  RS_NORMAL,
  RS_HIGH,
  RS_LOW,
} sn_rule_state_e;

typedef struct {
  sn_rule_state_e state;
  const sn_rule_desc_t *desc;
} sn_rule_instance_t;

static sn_rule_instance_t rule_instances[MAX_RULE];
static size_t rule_instance_len = 0;

void dispatch_command_and_print_result(const sn_command_t *commands) {
  cJSON *out = NULL;
  FOREACH_COMMAND(it, commands) {
    sn_dispatch_command_struct(it, &out);
    if (out) {
      char *response_str = cJSON_PrintUnformatted(out);
      ESP_LOGW(TAG, "%s", response_str);
      cJSON_free(response_str);
      cJSON_Delete(out);
    }
  }
}

static inline sn_rule_state_e sn_rule_eval_state(const sn_rule_desc_t *d, double value) {
  if (value < d->low) return RS_LOW;
  if (value > d->high) return RS_HIGH;
  return RS_NORMAL;
}

static inline void sn_run_exit(const sn_rule_desc_t *d, sn_rule_state_e old_state) {
  switch (old_state) {
    case RS_LOW:
      if (d->on_exit_low) dispatch_command_and_print_result(d->on_exit_low);
      break;
    case RS_NORMAL:
      if (d->on_exit_normal) dispatch_command_and_print_result(d->on_exit_normal);
      break;
    case RS_HIGH:
      if (d->on_exit_high) dispatch_command_and_print_result(d->on_exit_high);
      break;
  }
}

static inline void sn_run_entry(const sn_rule_desc_t *d, sn_rule_state_e new_state) {
  switch (new_state) {
    case RS_LOW:
      if (d->on_low) dispatch_command_and_print_result(d->on_low);
      break;
    case RS_NORMAL:
      if (d->on_normal) dispatch_command_and_print_result(d->on_normal);
      break;
    case RS_HIGH:
      if (d->on_high) dispatch_command_and_print_result(d->on_high);
      break;
  }
}

esp_err_t validate_rules(const sn_rule_desc_t *rule) {
  if (!rule) return ESP_FAIL;

  if (rule->id < LOCAL_ID_MIN || rule->id > LOCAL_ID_MAX) {
    ESP_LOGE(TAG, "id is invalid in rule", rule->id);
    return ESP_FAIL;
  }
  if (!rule->name) {
    ESP_LOGE(TAG, "name is missing in rule id=%d", rule->id);
    return ESP_FAIL;
  }
  if (!rule->on_high && !rule->on_low && !rule->on_normal) {
    ESP_LOGE(TAG, "no commands provided in rule id=%d (stale rules)", rule->id);
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t parse_rules() {
  rule_instance_len = 0;
  sn_rule_instance_t *inst = NULL;
  for (int i = 0; i < gRulesLen; i++) {
    if (validate_rules(&gRules[i]) == ESP_OK) {
      inst = &rule_instances[rule_instance_len];
      inst->state = RS_NORMAL;
      inst->desc = &gRules[i];
      rule_instance_len++;
    }
  }
  return ESP_OK;
}

void rule_engine_task(void *pvParams) {
  esp_err_t status = parse_rules();
  ESP_ERROR_CHECK_WITHOUT_ABORT(status);
  if (status != ESP_OK) {
    vTaskDelete(NULL);
  }
  QueueHandle_t queue = xQueueCreate(6, sizeof(sn_sensor_reading_t));
  register_consumer(queue);
  sn_sensor_reading_t reading;
  for (;;) {
    if (xQueueReceive(queue, &reading, portMAX_DELAY)) {
      // INFO: Use priority tree for scaling
      for (int i = 0; i < rule_instance_len; i++) {
        sn_rule_instance_t *rule = &rule_instances[i];
        if (rule->desc->src_id == reading.local_id) {
          sn_rule_state_e old_state = rule->state;
          sn_rule_state_e new_state = sn_rule_eval_state(rule->desc, reading.value);
          if (old_state == new_state) continue; // no transition, no commands
          sn_run_exit(rule->desc, old_state);
          sn_run_entry(rule->desc, new_state);
          rule->state = new_state;
        }
      }
    }
  }
}
