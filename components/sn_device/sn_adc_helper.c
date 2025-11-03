#include "sn_adc_helper.h"
#include "driver/adc_types_legacy.h"
#include "esp_log.h"
#include "esp_adc_cal.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include <stdlib.h>

static const char *TAG = "ADC_HELPER";

static SemaphoreHandle_t s_adc_mutex = NULL;

// We keep characteristics per attenuation (0/2.5/6/11 dB)
static esp_adc_cal_characteristics_t *s_chars = NULL;
static bool s_chars_ready = false;

// default global width & default attenuation
static const adc_bits_width_t DEFAULT_WIDTH = ADC_WIDTH_BIT_12;
static const adc_atten_t DEFAULT_ATTEN = ADC_ATTEN_DB_12;
static const uint32_t DEFAULT_VREF = 1100; // mV (used if no eFuse Vref)

static inline int atten_index(adc_atten_t a) {
  // ADC_ATTEN_DB_0 = 0, DB_2_5 =1, DB_6=2, DB_11=3
  return (int)a;
}

esp_err_t adc_helper_init(void) {
  if (s_adc_mutex == NULL) {
    s_adc_mutex = xSemaphoreCreateMutex();
    if (!s_adc_mutex) {
      ESP_LOGE(TAG, "Failed to create ADC mutex");
      return ESP_ERR_NO_MEM;
    }
  }

  // set ADC1 global width
  adc1_config_width(DEFAULT_WIDTH);

  // allocate characteristics for 4 attenuation levels
  if (!s_chars) {
    s_chars = calloc(4, sizeof(esp_adc_cal_characteristics_t));
    if (!s_chars) return ESP_ERR_NO_MEM;
  }

  // characterize for each attenuation
  for (int i = 0; i < 4; ++i) {
    esp_adc_cal_value_t val = esp_adc_cal_characterize(
      ADC_UNIT_1, (adc_atten_t)i, DEFAULT_WIDTH, DEFAULT_VREF, &s_chars[i]
    );
    ESP_LOGI(TAG, "ADC char att=%d: type=%d", i, val);
  }
  s_chars_ready = true;
  ESP_LOGI(TAG, "adc_helper initialized");
  return ESP_OK;
}

esp_err_t adc_helper_config_channel_atten(adc1_channel_t ch, adc_atten_t atten) {
  if (ch < ADC1_CHANNEL_0 || ch >= ADC1_CHANNEL_MAX) return ESP_ERR_INVALID_ARG;
  // caller must ensure adc_helper_init() called
  adc1_config_channel_atten(ch, atten);
  return ESP_OK;
}

int adc_helper_read_raw(adc1_channel_t ch) {
  if (ch < ADC1_CHANNEL_0 || ch >= ADC1_CHANNEL_MAX) return -1;
  if (!s_adc_mutex) return -1;
  if (xSemaphoreTake(s_adc_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) return -1;
  // perform single raw read
  int raw = adc1_get_raw(ch);
  xSemaphoreGive(s_adc_mutex);
  return raw;
}

int adc_helper_read_raw_avg(adc1_channel_t ch, int samples) {
  if (samples <= 0) samples = 1;
  long sum = 0;
  for (int i = 0; i < samples; ++i) {
    int r = adc_helper_read_raw(ch);
    if (r < 0) return r;
    sum += r;
    // small settlement delay to reduce channel switching artifacts
    esp_rom_delay_us(100); // microseconds; short busy wait (safe in small loops)
  }
  return (int)(sum / samples);
}

uint32_t adc_helper_raw_to_mv(adc1_channel_t ch, int raw) {
  (void)ch; // characteristic depends on configured attenuation, we assume default
  if (!s_chars_ready || !s_chars) return 0;
  // We don't know which attenuation the channel currently has here.
  // Use the highest attenuation characteristic (DB_11) by default for better dynamic range.
  return esp_adc_cal_raw_to_voltage(raw, &s_chars[atten_index(DEFAULT_ATTEN)]);
}

esp_err_t adc_helper_read_mv_avg(adc1_channel_t ch, int samples, uint32_t *out_mv) {
  if (!out_mv) return ESP_ERR_INVALID_ARG;
  int raw = adc_helper_read_raw_avg(ch, samples);
  if (raw < 0) return ESP_FAIL;
  *out_mv = adc_helper_raw_to_mv(ch, raw);
  return ESP_OK;
}

adc1_channel_t adc_helper_pin_to_channel(gpio_num_t pin) {
  switch (pin) {
    case GPIO_NUM_32:
      return ADC1_CHANNEL_4;
      break;
    case GPIO_NUM_33:
      return ADC1_CHANNEL_5;
      break;
    case GPIO_NUM_34:
      return ADC1_CHANNEL_6;
      break;
    case GPIO_NUM_35:
      return ADC1_CHANNEL_7;
      break;
    case GPIO_NUM_36:
      return ADC1_CHANNEL_0;
      break;
    case GPIO_NUM_39:
      return ADC1_CHANNEL_3;
      break;
    default:
      return ADC1_CHANNEL_MAX;
  }
}
