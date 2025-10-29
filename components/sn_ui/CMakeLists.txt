idf_component_register(
  SRCS "sn_ui.c"
  PRIV_REQUIRES esp_driver_i2c esp_lcd
  INCLUDE_DIRS "."
)
