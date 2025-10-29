#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "sn_capability.h"
#include "sn_driver.h"
#include "sn_driver/port_desc.h"
#include "sn_driver/sensor.h"
#include "sn_driver_registry.h"
#include "sn_error.h"
#include "sn_inet.h"
#include "sn_json.h"
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
#include <math.h>
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

#define WIFI_SSID CONFIG_ESP_WIFI_SSID
#define WIFI_PASS CONFIG_ESP_WIFI_PASSWORD

// clang-format off

// Define globals (device metadata)
static const sn_port_measurement_map_t dht1_map[] = {
  MEASUREMENT_MAP_ENTRY(0x01, ST_TEMPERATURE, "C"),
  MEASUREMENT_MAP_ENTRY(0x02, ST_HUMIDITY, "%"),
  MEASUREMENT_MAP_ENTRY_NULL()
};

static const sn_port_measurement_map_t soil1_map[] = {
  MEASUREMENT_MAP_ENTRY(0x03, ST_MOISTURE, "%"),
  MEASUREMENT_MAP_ENTRY_NULL(),
};

const sn_device_port_desc_t gDevicePorts[] = {
  // sensors (local_id defined in measurements map)
  SENSOR_PORT_LITERAL("soil-1", "soil_adc", ((sn_sensor_port_t) {
    .usage.gpio.pin = GPIO_NUM_34,
    .usage_type = PUT_GPIO,
    .measurements = soil1_map
  })),
  SENSOR_PORT_LITERAL("dht-1", "dht11", ((sn_sensor_port_t) {
    .usage.gpio.pin = GPIO_NUM_23,
    .usage_type = PUT_GPIO,
    .measurements = dht1_map
  })),
  // actuator
  ACTUATOR_PORT_LITERAL("led-yellow", "led", ((sn_actuator_port_t){
    .local_id = 0x0a,
    .usage.gpio.pin = GPIO_NUM_18,
    .usage_type = PUT_GPIO
  })),
  ACTUATOR_PORT_LITERAL("led-green", "led", ((sn_actuator_port_t){
    .local_id = 0x0b,
    .usage.gpio.pin = GPIO_NUM_19,
    .usage_type = PUT_GPIO
  })),
  ACTUATOR_PORT_LITERAL("pump-1", "relay", ((sn_actuator_port_t){
    .local_id = 0x0c,
    .usage.gpio.pin = GPIO_NUM_25,
    .usage_type = PUT_GPIO,
  })),
};
const size_t gDevicePortsLen = sizeof(gDevicePorts) / sizeof(sn_device_port_desc_t);
// clang-format on

// const command_capability_t gCommandCapabilities[] = {
//   {
//     .action = "set_sample_rate",
//     .params = (const char *[]){"tpms", NULL},
//     .handler = set_sample_rate_handler,
//   },
//   {
//     .action = "set_led_state",
//     .params = (const char *[]){"actuatorId", "enable", NULL},
//     .handler = led_handler,
//   },
//   {
//     .action = "set_pump_state",
//     .params = (const char *[]){"actuatorId", "enable", NULL},
//     .handler = pump_handler,
//   },
//   {
//     .action = "set_moist_pump_threshold",
//     .params = (const char *[]){"min", "max", NULL},
//     .handler = set_moist_pump_threshold,
//   },
//   {
//     .action = "restart",
//     .params = NULL,
//     .handler = restart_handler,
//   }
// };

esp_err_t init_device_config() {
  // Init cluster id
  if (CONFIG_CLUSTER_ID[0] != '\0') sn_storage_set_cluster_id(CONFIG_CLUSTER_ID);
  // Init organization id
  if (CONFIG_ORG_ID[0] != '\0') sn_storage_set_org_id(CONFIG_ORG_ID);

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
    cJSON *capabilities_json = device_ports_to_capabilities_json();
    char *payloadStr = cJSON_PrintUnformatted(capabilities_json);
    if (mqtt_registration_run(CONFIG_MQTT_BROKER_URI, payloadStr) != ESP_OK) {
      ESP_LOGE(TAG, "Register device failed restarting...");
      esp_restart();
    } else {
      sn_storage_get_device_id(ctx.deviceId, sizeof(ctx.deviceId));
      sn_mqtt_topic_cache_set_context(&ctx);
    }
    if (capabilities_json) {
      cJSON_free(payloadStr);
      cJSON_Delete(capabilities_json);
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

void sensor_poll_task() {
  size_t len = sn_driver_get_instance_len();
  sn_device_instance_t *instances = sn_driver_get_device_instances();

  char *reading_json_str = NULL;
  sn_sensor_reading_t readings[4];
  for (;;) {
    cJSON *reading_json = NULL;
    FOR_EACH_SENSOR_INSTANCE(it, instances, len) {
      int outcount = 0;
      if (it->online && it->driver->read_multi) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(
          it->driver->read_multi((void *)&it->ctx, readings, sizeof(readings), &outcount)
        );
      }
      for (int i = 0; i < outcount; i++) {
        reading_json = sensor_reading_to_json_obj(&readings[i]);
        if (reading_json) {
          reading_json_str = cJSON_PrintUnformatted(reading_json);
          puts(reading_json_str);
          if (reading_json_str) cJSON_free(reading_json_str);
          cJSON_Delete(reading_json);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

void app() {
  GOTO_IF_ESP_ERROR(end, sn_storage_init(NULL));
  GOTO_IF_ESP_ERROR(end, sn_inet_init(NULL));
  // Connect to the internet using wifi this will block and wait for the connection
  GOTO_IF_ESP_ERROR(end, sn_inet_wifi_connect(WIFI_SSID, WIFI_PASS));
  GOTO_IF_FALSE_MSG(end, sn_inet_is_connected(), TAG, "Network is not available");
  // Init sntp server and sync time
  GOTO_IF_ESP_ERROR(end, sn_init_sntp());
  GOTO_IF_ESP_ERROR(end, init_device_config());

  sn_storage_list_all();

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

// void led_driver_test() {
//   // clang-format off
//   static sn_device_port_desc_t ports[] = {
//     {
//       .port_name = "led_yellow",
//       .desc.a = {
//         .local_id = 0x0a,
//         .usage.gpio.pin = GPIO_NUM_18,
//         .usage_type = PUT_GPIO
//       },
//       .drv_name = "led",
//       .drv_type = DRIVER_TYPE_ACTUATOR,
//     },
//     {
//       .port_name = "led_green",
//       .desc.a = {
//         .local_id = 0x0b,
//         .usage.gpio.pin = GPIO_NUM_19,
//         .usage_type = PUT_GPIO
//       },
//       .drv_name = "led",
//       .drv_type = DRIVER_TYPE_ACTUATOR,
//     },
//   };
//   // clang-format on
//
//   cJSON *out = NULL;
//   const char *payload_json = "{"
//                              "\"brightness\":%.2f,"
//                              "\"enable\":true"
//                              "}";
//   char payload_formatted[128];
//   sn_driver_register(&led_driver);
//   sn_driver_bind_all_ports(ports, sizeof(ports) / sizeof(sn_device_port_desc_t));
//
//   size_t len = sn_driver_get_instance_len();
//   sn_device_instance_t *instances = sn_driver_get_device_instances();
//   for (;;) {
//     FOR_EACH_ACTUATOR_INSTANCE(it, instances, len) {
//       if (strncmp(it->driver->name, "led", 3) == 0) {
//         snprintf(payload_formatted, sizeof(payload_formatted), payload_json, 1.0f);
//         ESP_LOGI(TAG, "Sending payload to %s: %s", it->port->port_name, payload_formatted);
//         it->driver->control((void *)&it->ctx, payload_formatted, &out);
//
//         if (out) {
//           char *outstr = cJSON_PrintUnformatted(out);
//           puts(outstr);
//           cJSON_free(outstr);
//           cJSON_Delete(out);
//         }
//
//         vTaskDelay(pdMS_TO_TICKS(1000));
//         snprintf(payload_formatted, sizeof(payload_formatted), payload_json, 0.0f);
//         ESP_LOGI(TAG, "Sending payload to %s: %s", it->port->port_name, payload_formatted);
//         it->driver->control((void *)&it->ctx, payload_formatted, &out);
//
//         if (out) {
//           char *outstr = cJSON_PrintUnformatted(out);
//           puts(outstr);
//           cJSON_free(outstr);
//           cJSON_Delete(out);
//         }
//       }
//     }
//   }
// }

// void dht_driver_test() {
//   static const sn_port_measurement_map_t dht1_map[] = {
//     MEASUREMENT_MAP_ENTRY(0x01, ST_TEMPERATURE, "C"),
//     MEASUREMENT_MAP_ENTRY(0x02, ST_HUMIDITY, "%"),
//     MEASUREMENT_MAP_ENTRY_NULL(),
//   };
//   // clang-format off
//   static sn_device_port_desc_t ports[] = {
//     SENSOR_PORT_LITERAL("dht-1", "dht11", ((sn_sensor_port_t){
//       .usage.gpio.pin = GPIO_NUM_23,
//       .usage_type = PUT_GPIO,
//       .measurements = dht1_map
//     })),
//   };
//   // clang-format on
//
//   sn_driver_register(&dht_driver);
//   sn_driver_bind_all_ports(ports, sizeof(ports) / sizeof(sn_device_port_desc_t));
//
//   sensor_poll_task();
// }
//
// void adc_driver_test() {
//   static const sn_port_measurement_map_t soil1_map[] = {
//     MEASUREMENT_MAP_ENTRY(0x01, ST_MOISTURE, "%"),
//     MEASUREMENT_MAP_ENTRY_NULL(),
//   };
//   static const sn_port_measurement_map_t light1_map[] = {
//     MEASUREMENT_MAP_ENTRY(0x02, ST_LIGHT_INTENSITY, "lux"),
//     MEASUREMENT_MAP_ENTRY_NULL(),
//   };
//   // clang-format off
//   static sn_device_port_desc_t ports[] = {
//     SENSOR_PORT_LITERAL("soil-1", "soil_adc", ((sn_sensor_port_t){
//       .usage.gpio.pin = GPIO_NUM_35,
//       .usage_type = PUT_GPIO,
//       .measurements = soil1_map
//     })),
//     SENSOR_PORT_LITERAL("light-1", "lm393", ((sn_sensor_port_t){
//       .usage.gpio.pin = GPIO_NUM_34,
//       .usage_type = PUT_GPIO,
//       .measurements = light1_map
//     })),
//   };
//   // clang-format on
//
//   sn_driver_register(&soil_moisture_driver);
//   sn_driver_register(&light_intensity_driver);
//   sn_driver_bind_all_ports(ports, sizeof(ports) / sizeof(sn_device_port_desc_t));
//
//   sensor_poll_task();
// }

void app_main(void) { app(); }
