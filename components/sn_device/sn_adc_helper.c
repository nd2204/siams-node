#include "sn_adc_helper.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "ADC_HELPER";

static SemaphoreHandle_t s_mutex = NULL;
static adc_oneshot_unit_handle_t s_unit = NULL;
static adc_cali_handle_t s_cali = NULL;

// store per-channel attenuation so conversion uses the right calibration
static adc_atten_t s_channel_atten[ADC_CHANNEL_MAX];

static inline int atten_index(adc_atten_t a) { return (int)a; }

esp_err_t adc_helper_init(void) {
  if (s_unit) return ESP_OK;

  s_mutex = xSemaphoreCreateMutex();
  if (!s_mutex) return ESP_ERR_NO_MEM;

  adc_oneshot_unit_init_cfg_t unit_cfg = {
    .unit_id = ADC_UNIT_1,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_unit));

  // try to enable calibration (supported on most chips)
  adc_cali_curve_fitting_config_t cali_cfg = {
    .unit_id = ADC_UNIT_1,
    .atten = ADC_ATTEN_DB_11,
    .bitwidth = ADC_BITWIDTH_12,
  };
  esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "ADC calibration enabled");
  } else {
    s_cali = NULL;
    ESP_LOGW(TAG, "ADC calibration not available, using raw values");
  }

  for (int i = 0; i < ADC_CHANNEL_MAX; ++i)
    s_channel_atten[i] = ADC_ATTEN_DB_11; // default

  return ESP_OK;
}

esp_err_t adc_helper_deinit(void) {
  if (s_cali) {
    adc_cali_delete_scheme_curve_fitting(s_cali);
    s_cali = NULL;
  }
  if (s_unit) {
    adc_oneshot_del_unit(s_unit);
    s_unit = NULL;
  }
  if (s_mutex) {
    vSemaphoreDelete(s_mutex);
    s_mutex = NULL;
  }
  return ESP_OK;
}

adc_channel_t adc_helper_pin_to_channel(gpio_num_t pin) {
  switch (pin) {
    case GPIO_NUM_32:
      return ADC_CHANNEL_4;
    case GPIO_NUM_33:
      return ADC_CHANNEL_5;
    case GPIO_NUM_34:
      return ADC_CHANNEL_6;
    case GPIO_NUM_35:
      return ADC_CHANNEL_7;
    case GPIO_NUM_36:
      return ADC_CHANNEL_0;
    case GPIO_NUM_37:
      return ADC_CHANNEL_1;
    case GPIO_NUM_38:
      return ADC_CHANNEL_2;
    case GPIO_NUM_39:
      return ADC_CHANNEL_3;
    default:
      return ADC_CHANNEL_MAX;
  }
}

esp_err_t adc_helper_config_channel(gpio_num_t pin, adc_atten_t atten, adc_channel_t *out_ch) {
  if (!s_unit) return ESP_ERR_INVALID_STATE;
  adc_channel_t ch = adc_helper_pin_to_channel(pin);
  if (ch == ADC_CHANNEL_MAX) return ESP_ERR_INVALID_ARG;

  adc_oneshot_chan_cfg_t chan_cfg = {
    .atten = atten,
    .bitwidth = ADC_BITWIDTH_12,
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(s_unit, ch, &chan_cfg));
  s_channel_atten[ch] = atten;

  if (out_ch) *out_ch = ch;
  return ESP_OK;
}

esp_err_t adc_helper_read_raw(adc_channel_t ch, int *out_raw) {
  if (!out_raw) return ESP_ERR_INVALID_ARG;
  if (!s_unit) return ESP_ERR_INVALID_STATE;
  if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;
  esp_err_t res = adc_oneshot_read(s_unit, ch, out_raw);
  xSemaphoreGive(s_mutex);
  return res;
}

esp_err_t adc_helper_read_raw_avg(adc_channel_t ch, int samples, int *out_raw) {
  if (!out_raw) return ESP_ERR_INVALID_ARG;
  if (samples <= 0) samples = 1;
  long sum = 0;
  int val;
  for (int i = 0; i < samples; ++i) {
    if (adc_helper_read_raw(ch, &val) != ESP_OK) return ESP_FAIL;
    sum += val;
    ets_delay_us(80);
  }
  *out_raw = (int)(sum / samples);
  return ESP_OK;
}

esp_err_t adc_helper_read_mv_avg(adc_channel_t ch, int samples, uint32_t *out_mv) {
  if (!out_mv) return ESP_ERR_INVALID_ARG;
  int raw;
  ESP_RETURN_ON_ERROR(adc_helper_read_raw_avg(ch, samples, &raw), TAG, "raw read failed");
  if (s_cali) {
    ESP_RETURN_ON_ERROR(
      adc_cali_raw_to_voltage(s_cali, raw, (int *)out_mv), TAG, "calibration convert failed"
    );
  } else {
    // crude fallback: scale 0..4095 => 0..3300 mV
    *out_mv = (uint32_t)((raw * 3300) / 4095);
  }
  return ESP_OK;
}
