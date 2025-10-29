#include "driver/gpio.h"
#include "sn_device.h"
#include "sn_device.h"
#include "sn_error.h"
#include "sn_inet.h"
#include "sn_json_validator.h"
#include "sn_mqtt_registration_client.h"
#include "sn_mqtt_router.h"
#include "sn_sntp.h"
#include "sn_storage.h"
#include "sn_mqtt_manager.h"
#include "handlers/sn_handlers.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include "cJSON.h"
#include "sdkconfig.h"
#include "sn_topic.h"
#include <sys/time.h>
#include <time.h>

static const char *TAG = "MAIN";

// void demo_ui(void *params) {
//   ESP_LOGI(TAG, "Display LVGL Scroll Text");
//   lv_display_t *disp = sn_ui_get_display();
//   lv_obj_t *scr = lv_display_get_screen_active(disp);
//   lv_obj_t *label = lv_label_create(scr);
//   lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll */
//   lv_label_set_text(label, "Hello cruel world");
//   /* Size of the screen (if you use rotation 90 or 270, please use
//    * lv_display_get_vertical_resolution) */
//   lv_obj_set_width(label, lv_display_get_horizontal_resolution(disp));
//   lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
// }

#define WIFI_SSID  CONFIG_ESP_WIFI_SSID
#define WIFI_PASS  CONFIG_ESP_WIFI_PASSWORD
#define BASE_TOPIC "org/" CONFIG_ORG_ID "/cluster/" CONFIG_CLUSTER_ID "/"

// Define globals (device metadata)
const sensor_capability_t gSensorCapabilities[] = {
  {.localId = 0x01, .type = "soil-moist",  .unit = "RH%", .sampleRateMs = 1000},
  {.localId = 0x02, .type = "temperature", .unit = "C",   .sampleRateMs = 1000},
  {.localId = 0x03, .type = "humidity",    .unit = "RH%", .sampleRateMs = 1000},
  {.localId = 0x04, .type = "light",       .unit = "lux", .sampleRateMs = 1000},
};

const actuator_capability_t gActuatorCapabilities[] = {
  {.localId = 0x09, .type = "pump",       .pin = GPIO_NUM_0 },
  {.localId = 0x0a, .type = "led_green",  .pin = GPIO_NUM_19},
  {.localId = 0x0b, .type = "led_yellow", .pin = GPIO_NUM_18},
};

const command_handler_entry_t gCommandHandlers[] = {
  {.localId = 0xfd, .handler = pump_handler},
  {.localId = 0xfe, .handler = led_handler },
};

// clang-format off
const command_capability_t gCommandCapabilities[] = {
  {
    .localId = 0xfe,
    .action = "set_led_state",
    .params = (const char *[]){"actuatorId", "enable"},
    .params_len = 2
  },
  {
    .localId = 0xfd,
    .action = "set_pump_state",
    .params = (const char *[]){"actuatorId", "enable"},
    .params_len = 2
  },
  {
    .action = "set_moist_pump_threshold",
    .params = (const char *[]){"min", "max"},
    .params_len = 2
  }
};
// clang-format on

const size_t SENSOR_COUNT = sizeof(gSensorCapabilities) / sizeof(sensor_capability_t);
const size_t ACTUATOR_COUNT = sizeof(gActuatorCapabilities) / sizeof(actuator_capability_t);
const size_t COMMAND_COUNT = sizeof(gCommandCapabilities) / sizeof(command_capability_t);
const size_t HANDLER_COUNT = sizeof(gCommandHandlers) / sizeof(command_handler_entry_t);

esp_err_t init_device_config() {
  // Init cluster id
  if (CONFIG_CLUSTER_ID[0] != '\0') sn_storage_set_cluster_id(CONFIG_CLUSTER_ID);
  // Init organization id
  if (CONFIG_ORG_ID[0] != '\0') sn_storage_set_org_id(CONFIG_ORG_ID);

  // Init gpio for led
  gpio_config_t io_conf;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (BIT(GPIO_NUM_18) | BIT(GPIO_NUM_19));
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&io_conf);

  sn_mqtt_topic_context_t ctx = {
    .orgId = CONFIG_ORG_ID,
    .clusterId = CONFIG_CLUSTER_ID,
  };
  gen_temp_id(ctx.deviceId, sizeof(ctx.deviceId));
  sn_mqtt_topic_cache_init(&ctx);

  // sn_storage_get_org_id(ctx.orgId, sizeof(ctx.orgId));
  // sn_storage_get_cluster_id(ctx.clusterId, sizeof(ctx.clusterId));

  // Init device id from storage
  if (sn_storage_get_device_id(ctx.deviceId, sizeof(ctx.deviceId)) != ESP_OK) {
    // if not exist then register
    ESP_LOGW(TAG, "Device not registered, running registration...");
    if (mqtt_registration_run(CONFIG_MQTT_BROKER_URI) != ESP_OK) {
      ESP_LOGE(TAG, "Register device failed restarting...");
      esp_restart();
    } else {
      sn_storage_get_device_id(ctx.deviceId, sizeof(ctx.deviceId));
      sn_mqtt_topic_cache_set_context(&ctx);
    }
  } else {
    // TODO:
    // verify device if deviceId exists in storage
  }
  print_topic_cache();

  return ESP_OK;
}

// mqtt command callback
static void on_command_msg(const char *topic, const char *payload) {
  ESP_LOGI("CMD", "Command received: %s", payload);
  cJSON *out = NULL;
  dispatch_command(payload, &out);
  if (out) {
    char *payload_str = cJSON_PrintUnformatted(out);
    sn_mqtt_publish("topic", payload_str, 1, true);
    cJSON_free(payload_str);
    cJSON_Delete(out);
  }
}

void app_main(void) {
  GOTO_IF_ESP_ERROR(end, sn_storage_init(NULL));
  GOTO_IF_ESP_ERROR(end, sn_inet_init(NULL));
  // Connect to the internet using wifi this will block and wait for the connection
  GOTO_IF_ESP_ERROR(end, sn_inet_wifi_connect(WIFI_SSID, WIFI_PASS));
  GOTO_IF_FALSE_MSG(end, sn_inet_is_connected(), TAG, "Network is not available");
  // Init sntp server and sync time
  GOTO_IF_ESP_ERROR(end, sn_init_sntp());
  GOTO_IF_ESP_ERROR(end, init_device_config());

  sn_storage_list_all();

  {
    cJSON *out = NULL;
    const char *payload_json = "{"
                               "  \"action\": \"set_led_state\","
                               "  \"params\": {"
                               "    \"actuatorId\": 11,"
                               "    \"enable\": true"
                               "  }"
                               "}";
    dispatch_command(payload_json, &out);
    char *outStr = cJSON_PrintUnformatted(out);
    ESP_LOGI(TAG, "%s", outStr);
    cJSON_free(outStr);
    cJSON_Delete(out);
  }

  // create mqtt client
  const sn_mqtt_topic_cache_t *cache = sn_mqtt_topic_cache_get();
  {

    // create timestamp
    char isots[64];
    sn_get_iso8601_timestamp(isots, sizeof(isots));

    // create lwt payload from timestamp
    cJSON *payload = create_lwt_payload_json(isots);
    char *payload_str = cJSON_PrintUnformatted(payload);

    // create config and initialize
    sn_mqtt_config_t conf =
      {.uri = CONFIG_MQTT_BROKER_URI, .lwt_topic = cache->status_topic, .lwt_payload = payload_str};
    sn_mqtt_init(&conf);

    // clean up
    cJSON_free(payload_str);
    cJSON_Delete(payload);
  }

  sn_mqtt_start();
  sn_mqtt_router_subscriber_add(cache->command_topic, on_command_msg, 1);

  // Chờ kết nối rồi publish thử
  vTaskDelay(pdMS_TO_TICKS(3000));

end:
  (void)0;
}
