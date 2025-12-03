idf_component_register(
  SRCS "sn_security.c"
  PRIV_REQUIRES nvs_flash sn_domain sn_storage mbedtls efuse sn_inet
  INCLUDE_DIRS "."
)
