idf_component_register(
  SRC_DIRS "src"
  REQUIRES esp_driver_gpio sn_storage sn_inet sn_mqtt_manager sn_domain sn_device sn_security
  INCLUDE_DIRS "."
)
