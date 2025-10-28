#include "misc/lv_async.h"
#include "sn_config.h"
#include "sn_devmgr.h"
#include "sn_sysmgr.h"
#include "sn_ui.h"
#include "sn_wifi.h"

#include "sdkconfig.h"
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/param.h>

#include "sn_sensors.h"

static const char *TAG = "main";

#define I2C_PORT I2C_NUM_0

void demo_ui(void *params) {
  ESP_LOGI(TAG, "Display LVGL Scroll Text");
  lv_display_t *disp = sn_ui_get_display();
  lv_obj_t *scr = lv_display_get_screen_active(disp);
  lv_obj_t *label = lv_label_create(scr);
  lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll */
  lv_label_set_text(label, "Hello cruel world");
  /* Size of the screen (if you use rotation 90 or 270, please use
   * lv_display_get_vertical_resolution) */
  lv_obj_set_width(label, lv_display_get_horizontal_resolution(disp));
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

#include "mqtt_client.h"
static void log_error_if_nonzero(const char *message, int error_code) {
  if (error_code != 0) {
    ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
  }
}

static void mqtt_event_handler(
  void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data
) {
  ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
  esp_mqtt_event_handle_t event = event_data;
  esp_mqtt_client_handle_t client = event->client;
  int msg_id;
  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
      msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
      ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
      msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
      ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

      msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
      ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

      msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
      ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
      break;
    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
      break;
    case MQTT_EVENT_SUBSCRIBED:
      ESP_LOGI(
        TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d, return code=0x%02x ", event->msg_id,
        (uint8_t)*event->data
      );
      msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
      ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_PUBLISHED:
      ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_DATA:
      ESP_LOGI(TAG, "MQTT_EVENT_DATA");
      printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
      printf("DATA=%.*s\r\n", event->data_len, event->data);
      break;
    case MQTT_EVENT_ERROR:
      ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
      if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
        log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
        log_error_if_nonzero(
          "captured as transport's socket errno", event->error_handle->esp_transport_sock_errno
        );
        ESP_LOGI(
          TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno)
        );
      }
      break;
    default:
      ESP_LOGI(TAG, "Other event id:%d", event->event_id);
      break;
  }
}

static void mqtt_app_start(void) {
  const esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = CONFIG_MQTT_BROKER_URI,
  };

  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  /* The last argument may be used to pass data to the event handler, in this
   * example mqtt_event_handler */
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  esp_mqtt_client_start(client);
}

void network_task(void *params) {
  esp_err_t ret = sn_wifi_connect(CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
  if (ret == ESP_OK) {
    mqtt_app_start();
  }
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void app_main(void) {
  sn_sysmgr_init_self();
  SN_SYSMGR_REGISTER("sn-devmgr", sn_devmgr_init, NULL);
  SN_SYSMGR_REGISTER("sn-sensors", sn_sensors_init, NULL);
  SN_SYSMGR_REGISTER("sn-ui", sn_ui_init, NULL);
  SN_SYSMGR_REGISTER("sn-wifi", sn_wifi_init, NULL);
  sn_sysmgr_init_all();

  if (sn_sysmgr_is_valid()) {

    xTaskCreatePinnedToCore(
      ui_task, "ui-task", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL, 0
    );
    // xTaskCreatePinnedToCore(read_bh1750_task, "light-task", 4096, NULL, 5,
    // NULL, 1);
    xTaskCreatePinnedToCore(read_dht_task, "dht-task", 4096, NULL, 5, NULL, 1);
    xTaskCreate(network_task, "network-task", 4096, NULL, 5, NULL);

    lv_async_call(demo_ui, NULL);
  }

  sn_sysmgr_shutdown_all();
}
