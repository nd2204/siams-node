#ifndef SN_ERROR_H
#define SN_ERROR_H

#include <esp_err.h>
#include <esp_log.h>

#define TRY(expr)                                                                                  \
  do {                                                                                             \
    esp_err_t _err = (expr);                                                                       \
    if (_err != ESP_OK) {                                                                          \
      ESP_ERROR_CHECK_WITHOUT_ABORT(_err);                                                         \
      return _err;                                                                                 \
    }                                                                                              \
  } while (0)

#define RETURN_IF_FALSE(cond, ret_val)                                                             \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      return (ret_val);                                                                            \
    }                                                                                              \
  } while (0)

#define RETURN_IF_FALSE_MSG(TAG, cond, ret_val, msg, ...)                                          \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      ESP_LOGE(TAG, msg, ##__VA_ARGS__);                                                           \
      return (ret_val);                                                                            \
    }                                                                                              \
  } while (0)

#define GOTO_IF_FALSE(goto_label, cond)                                                            \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      goto goto_label;                                                                             \
    }                                                                                              \
  } while (0)

#define GOTO_IF_ESP_ERROR(goto_label, expr)                                                        \
  do {                                                                                             \
    esp_err_t _err = (expr);                                                                       \
    if (_err != ESP_OK) {                                                                          \
      ESP_ERROR_CHECK_WITHOUT_ABORT(_err);                                                         \
      goto goto_label;                                                                             \
    }                                                                                              \
  } while (0)

#define GOTO_IF_FALSE_MSG(goto_label, cond, tag, msg, ...)                                         \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      ESP_LOGE(TAG, msg, ##__VA_ARGS__);                                                           \
      goto goto_label;                                                                             \
    }                                                                                              \
  } while (0)

#endif // !SN_ERROR_H
