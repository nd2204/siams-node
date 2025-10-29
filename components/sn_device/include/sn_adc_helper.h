// adc_helper.h
#pragma once
#include "esp_err.h"
#include "driver/adc.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize helper (call once at startup)
esp_err_t adc_helper_init(void);

// Configure attenuation for a channel (optional; helper will set default if not)
esp_err_t adc_helper_config_channel_atten(adc1_channel_t ch, adc_atten_t atten);

// Read raw ADC value from ADC1 channel (0..4095) or return negative on error
int adc_helper_read_raw(adc1_channel_t ch);

// Read averaged raw ADC value (samples > 0)
int adc_helper_read_raw_avg(adc1_channel_t ch, int samples);

// Convert raw sample -> millivolts using esp_adc_cal characteristic
uint32_t adc_helper_raw_to_mv(adc1_channel_t ch, int raw);

// Convenience: read mv averaged
esp_err_t adc_helper_read_mv_avg(adc1_channel_t ch, int samples, uint32_t *out_mv);

// Helper: map a GPIO pin to ADC1 channel (returns -1 if not supported)
adc1_channel_t adc_helper_pin_to_channel(gpio_num_t pin);

#ifdef __cplusplus
}
#endif
