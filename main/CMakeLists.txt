file(GLOB_RECURSE COMP_SRCS "*.c")

idf_component_register(
  SRCS "main.c" "sn_ui.c" "sn_devmgr.c" "sn_wifi.c"
  INCLUDE_DIRS "."
  PRIV_REQUIRES esp_driver_i2c esp_lcd esp_wifi nvs_flash
)
