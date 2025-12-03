#ifndef SN_MQTT_MANAGER_H
#define SN_MQTT_MANAGER_H

#include "cJSON.h"
#include "esp_err.h"
#include "sn_driver/sensor.h"
#include <stdbool.h>

// Callback type
typedef void (*sn_mqtt_msg_cb_t)(const char *topic, const char *payload, void *arg);

// Config struct (optional external)
typedef struct {
  const char *uri;         // e.g. "mqtt://192.168.1.10"
  const char *lwt_topic;   // LWT topic
  const char *lwt_payload; // e.g. {"status":"offline"}
} sn_mqtt_config_t;

/*
 * @brief Initialize the mqtt client from config
 */
esp_err_t sn_mqtt_init(const sn_mqtt_config_t *cfg);

/*
 * @brief start the mqtt client
 */
esp_err_t sn_mqtt_start(void);

/*
 * @brief enqueue publish payload to mqtt topic
 */
esp_err_t sn_mqtt_publish_enqueue(const char *topic, const char *payload, int qos, bool retain);

/*
 * @brief general publish payload method without signature
 */
esp_err_t sn_mqtt_publish_payload_json(cJSON *payload, const char *topic);

/*
 * @brief general publish payload method with signature
 */
esp_err_t sn_mqtt_publish_json_payload_signed(
  cJSON *payload, const char *topic, int qos, bool retain
);

/*
 * @brief Subscribe to mqtt topic
 */
esp_err_t sn_mqtt_subscribe(const char *topic, int qos);

/*
 * @brief Register message handler
 */
esp_err_t sn_mqtt_register_handler(sn_mqtt_msg_cb_t cb, void *arg);

/*
 * @brief stop the client
 */
esp_err_t sn_mqtt_stop();

/*
 * @brief Check if client is connected
 */
esp_err_t sn_mqtt_destroy();

/*
 * @brief debug utils
 */
void mqtt_debug_print_rx(const void *event);

#endif // !SN_MQTT_MANAGER_H
