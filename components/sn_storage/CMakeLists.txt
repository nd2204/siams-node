idf_component_register(
  SRCS "sn_storage.c"
  PRIV_REQUIRES nvs_flash sn_domain
  INCLUDE_DIRS "."
)
