#include "forward.h"
#include "sn_driver.h"
#include "sn_driver/driver_inst.h"
#include "sn_driver_registry.h"
#include "sn_json.h"

static const char *TAG = "sensor_control";
static const char *sensor_control_types[] = {"sensor_control", ((void *)0)};
static const sn_param_desc_t params_desc[] = {
  {.name = "local_id",
   .type = PTYPE_INT,
   .required = true,
   .min = LOCAL_ID_MIN,
   .max = LOCAL_ID_MAX},
  {.name = "rate_ms", .type = PTYPE_NUMBER, .required = true, .min = 1000, .max = UINT64_MAX},
  {.name = ((void *)0)}
};

static const sn_command_desc_t schema = {
  .action = "set_sample_rate",
  .params = params_desc,
};

static _Bool sensor_control_probe(const sn_device_port_desc_t *port) { return true; }

static esp_err_t sensor_control_init(
  const sn_device_port_desc_t *desc, void *ctx_out, size_t ctx_size
) {
  return ESP_OK;
}

static void sensor_control_deinit(void *ctx) { (void)ctx; }

static esp_err_t sensor_control_controller(
  void *ctxv, const cJSON *paramsJson, cJSON **out_result
) {
  if (!paramsJson) return ESP_ERR_INVALID_ARG;
  if (!validate_params_json(params_desc, paramsJson, out_result)) {
    return ESP_ERR_INVALID_ARG;
  }

  int id = INVALID_LOCAL_ID;
  double sample_rate = 0;
  if (!json_get_int(paramsJson, "local_id", &id)) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!json_get_number(paramsJson, "rate_ms", &sample_rate)) {
    if (out_result) *out_result = build_error_fmt("invalid params rate_ms");
    return ESP_ERR_INVALID_ARG;
  }

  sn_device_instance_t *inst = sn_find_instance_by_local_id(id);
  if (!inst) {
    if (out_result) *out_result = build_success_fmt("local id not found");
    return ESP_FAIL;
  }

  sn_device_instance_set_interval(inst, sample_rate);

  if (out_result)
    *out_result = build_success_fmt("%s sample_rate=%d", inst->port->port_name, inst->interval_ms);
  return ESP_OK;
}

const sn_driver_desc_t sensor_control_driver = {
  .name = "sensor_control_drv",
  .supported_types = sensor_control_types,
  .priority = 50,
  .probe = NULL,
  .init = sensor_control_init,
  .deinit = sensor_control_deinit,
  .read_multi = NULL,
  .control = sensor_control_controller,
  .command_desc = &schema
};
