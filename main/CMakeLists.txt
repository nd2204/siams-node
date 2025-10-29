idf_component_register(
  SRC_DIRS handlers "."
  REQUIRES esp_driver_gpio sn_storage sn_inet sn_mqtt_manager sn_domain sn_device
  INCLUDE_DIRS "."
)
