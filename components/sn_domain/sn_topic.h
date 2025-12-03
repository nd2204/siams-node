#ifndef SN_MQTT_TOPIC_H
#define SN_MQTT_TOPIC_H

#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "forward.h"

#define MAX_TOPIC_LEN 512

#define TOPIC_BASE_FMT           "org/%s/"
#define TOPIC_DEV_FMT            "org/%s/device/%s/"
#define TOPIC_REGISTER_FMT       TOPIC_BASE_FMT "register/%s"
#define TOPIC_REGISTER_ACK_FMT   TOPIC_BASE_FMT "register-ack/%s"
#define TOPIC_RECOVER_FMT        TOPIC_BASE_FMT "recover/%s"
#define TOPIC_RECOVER_ACK_FMT    TOPIC_BASE_FMT "recover-ack/%s"
#define TOPIC_DEV_VERIFY_FMT     TOPIC_DEV_FMT "verify"
#define TOPIC_DEV_VERIFY_ACK_FMT TOPIC_DEV_FMT "verify-ack"

// clang-format off
#define TOPIC_DEVICE_CONFIG(X) \
  X(telemetry,    TOPIC_DEV_FMT "telemetry")      \
  X(status,       TOPIC_DEV_FMT "status")         \
  X(command,      TOPIC_DEV_FMT "command")        \
  X(command_ack,  TOPIC_DEV_FMT "command-ack")    \
  X(event,        TOPIC_DEV_FMT "event")
// clang-format on

typedef struct {
  char orgId[64];
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

void sn_build_device_topic_from_ctx(char *buf, size_t len, const char *fmt);

static inline void sn_build_device_topic(
  char *buf, size_t len, const char *fmt, const char *org, const char *device
) {
  int n = snprintf(buf, len, (fmt), org, device);
  if (n < 0 || n >= len) {
    ESP_LOGE("SN_TOPIC", "topic truncated");
    return;
  }
}

const sn_mqtt_topic_context_t *sn_mqtt_topic_cache_get_context(void);

// return the topic cache (never NUll)
const sn_mqtt_topic_cache_t *sn_mqtt_topic_cache_get(void);

// debug
void print_topic_cache(void);

//--------------------------------------------------------------------------------
// payload builder helpers for topics
//--------------------------------------------------------------------------------
cJSON *create_lwt_payload_json();

cJSON *create_telemetry_payload_json(local_id_t localId, double value, const char *isots);

#endif // !SN_MQTT_TOPICS_H
