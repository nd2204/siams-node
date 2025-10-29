idf_component_register(
  SRC_DIRS "."
  PRIV_REQUIRES
    sn_domain esp_timer esp_driver_gpio esp_driver_ledc 
    esp_adc
  INCLUDE_DIRS "." "include"
)
