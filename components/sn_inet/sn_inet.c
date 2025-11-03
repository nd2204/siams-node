#include "sn_inet.h"
#include "esp_event_base.h"
#include "freertos/projdefs.h"
#include "sn_error.h"

#include <sdkconfig.h>
#include <esp_wifi_types_generic.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <string.h>

#if CONFIG_ESP_STATION_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define H2E_IDENTIFIER    ""
#elif CONFIG_ESP_STATION_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define H2E_IDENTIFIER    CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_STATION_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define H2E_IDENTIFIER    CONFIG_ESP_WIFI_PW_ID
#endif // CONFIG_ESP_STATION_WPA3_SAE_PWE_HUNT_AND_PECK

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif // CONFIG_ESP_WIFI_AUTH_OPEN

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static const char *TAG = "SN_INET";
static int sRetryNum = 0;
static bool sWifiSystemActive = 0;
static bool sIsConnected = 0;

static void event_handler(
  void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data
) {
  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_STA_START) {
      esp_wifi_connect();
    } else if (WIFI_EVENT_STA_DISCONNECTED) {
      if (sRetryNum < CONFIG_ESP_MAXIMUM_RETRY) {
        esp_wifi_connect();
        sRetryNum++;
        ESP_LOGI(TAG, "Retrying... (%d/%d)", sRetryNum, CONFIG_ESP_MAXIMUM_RETRY);
      } else {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        ESP_LOGE(TAG, "Connect to the AP fail");
      }
      fflush(stdout);
    }
  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
      sRetryNum = 0;
      xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    if (event_id == IP_EVENT_STA_LOST_IP) {
      ESP_LOGI(TAG, "Lost connection to the ap");
    }
  }
}

// Allow lazy loading a subsystem
static esp_err_t sn_init_wifi() {
  // Only log error, warning for wifi
  esp_log_level_set("wifi", ESP_LOG_WARN);
  esp_log_level_set("wifi_init", ESP_LOG_WARN);
  // Init wifi event group
  s_wifi_event_group = xEventGroupCreate();

  // Init wifi driver
  fflush(stdout);
  vTaskDelay(pdMS_TO_TICKS(200));
  TRY(esp_netif_init());
  TRY(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  TRY(esp_wifi_init(&cfg));

  ESP_LOGI(TAG, "WIFI initialized");
  sWifiSystemActive = 1;
  return ESP_OK;
}

esp_err_t sn_inet_init(void *args) {
  bool lazy = args && (bool)args;
  if (!lazy) {
    TRY(sn_init_wifi());
  }
  return ESP_OK;
}

esp_err_t sn_inet_wifi_connect(const char *ssid, const char *password) {
  if (!sWifiSystemActive) sn_init_wifi();
  vTaskDelay(pdMS_TO_TICKS(1000)); // wait for init

  if (sIsConnected) return ESP_OK;
  // Assign wifi event handlers
  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  TRY(esp_event_handler_instance_register(
    WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id
  ));
  TRY(esp_event_handler_instance_register(
    IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip
  ));

  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
  // clang-format off
  wifi_config_t wifi_config = {
    .sta = {
      .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
      .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
      .sae_h2e_identifier = H2E_IDENTIFIER,
    },
  };
  // clang-format on

  memcpy(wifi_config.sta.ssid, ssid, sizeof(uint8_t) * 32);
  memcpy(wifi_config.sta.password, password, sizeof(uint8_t) * 64);

  TRY(esp_wifi_set_mode(WIFI_MODE_STA));
  TRY(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  TRY(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
   * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
   * bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(
    s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY
  );

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we
   * can test which event actually happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s", CONFIG_ESP_WIFI_SSID);
    sIsConnected = 1;
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID: %s", CONFIG_ESP_WIFI_SSID);
    sIsConnected = 0;
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
  vEventGroupDelete(s_wifi_event_group);

  return ESP_OK;
}

bool sn_inet_is_connected() { return sIsConnected; }
