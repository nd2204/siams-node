idf_component_register(
  SRC_DIRS "."
  PRIV_REQUIRES
    sn_domain sn_inet esp_timer esp_driver_gpio esp_driver_ledc 
    esp_adc esp_lcd esp_driver_i2c mbedtls efuse
  INCLUDE_DIRS "." "include"
)
