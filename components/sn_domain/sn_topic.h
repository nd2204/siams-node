#ifndef SN_MQTT_TOPIC_H
#define SN_MQTT_TOPIC_H

#include "cJSON.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "sn_driver/port_desc.h"

#define MAX_TOPIC_LEN 256

#define TOPIC_BASE_FMT         "org/%s/cluster/%s/"
#define TOPIC_DEV_FMT          "org/%s/cluster/%s/device/%s/"
#define TOPIC_REGISTER_FMT     TOPIC_BASE_FMT "register/%s"
#define TOPIC_REGISTER_ACK_FMT TOPIC_BASE_FMT "register-ack/%s"

// clang-format off
#define TOPIC_DEVICE_CONFIG(X) \
  X(verify,       TOPIC_DEV_FMT "verify")         \
  X(verify_ack,   TOPIC_DEV_FMT "verify-ack")     \
  X(telemetry,    TOPIC_DEV_FMT "telemetry")      \
  X(status,       TOPIC_DEV_FMT "status")         \
  X(command,      TOPIC_DEV_FMT "command")        \
  X(command_ack,  TOPIC_DEV_FMT "command-ack")    \
  X(event,        TOPIC_DEV_FMT "event")
// clang-format on

typedef struct {
  char orgId[64];
  char clusterId[64];
  char deviceId[64];
} sn_mqtt_topic_context_t;

typedef struct {
#define GEN_TOPIC_CACHE_FIELD(NAME, fmt, ...) char NAME##_topic[MAX_TOPIC_LEN];
  TOPIC_DEVICE_CONFIG(GEN_TOPIC_CACHE_FIELD)
#undef GEN_TOPIC_CACHE_FIELD
} sn_mqtt_topic_cache_t;

//--------------------------------------------------------------------------------
// API
//--------------------------------------------------------------------------------
esp_err_t sn_mqtt_topic_cache_init(const sn_mqtt_topic_context_t *ctx);

esp_err_t sn_mqtt_topic_cache_set_context(const sn_mqtt_topic_context_t *ctx);

const sn_mqtt_topic_context_t *sn_mqtt_topic_cache_get_context(void);

const sn_mqtt_topic_cache_t *sn_mqtt_topic_cache_get(void);

// debug
void print_topic_cache(void);

//--------------------------------------------------------------------------------
// payload builder helpers for topics
//--------------------------------------------------------------------------------
cJSON *create_lwt_payload_json(const char *isots);

cJSON *create_telemetry_payload_json(local_id_t localId, double value, const char *isots);

#endif // !SN_MQTT_TOPICS_H
