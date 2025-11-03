// void led_driver_test() {
//   // clang-format off
//   static sn_device_port_desc_t ports[] = {
//     {
//       .port_name = "led_yellow",
//       .desc.a = {
//         .local_id = 0x0a,
//         .usage.gpio.pin = GPIO_NUM_18,
//         .usage_type = PUT_GPIO
//       },
//       .drv_name = "led",
//       .drv_type = DRIVER_TYPE_ACTUATOR,
//     },
//     {
//       .port_name = "led_green",
//       .desc.a = {
//         .local_id = 0x0b,
//         .usage.gpio.pin = GPIO_NUM_19,
//         .usage_type = PUT_GPIO
//       },
//       .drv_name = "led",
//       .drv_type = DRIVER_TYPE_ACTUATOR,
//     },
//   };
//   // clang-format on
//
//   cJSON *out = NULL;
//   const char *payload_json = "{"
//                              "\"brightness\":%.2f,"
//                              "\"enable\":true"
//                              "}";
//   char payload_formatted[128];
//   sn_driver_register(&led_driver);
//   sn_driver_bind_all_ports(ports, sizeof(ports) / sizeof(sn_device_port_desc_t));
//
//   size_t len = sn_driver_get_instance_len();
//   sn_device_instance_t *instances = sn_driver_get_device_instances();
//   for (;;) {
//     FOR_EACH_ACTUATOR_INSTANCE(it, instances, len) {
//       if (strncmp(it->driver->name, "led", 3) == 0) {
//         snprintf(payload_formatted, sizeof(payload_formatted), payload_json, 1.0f);
//         ESP_LOGI(TAG, "Sending payload to %s: %s", it->port->port_name, payload_formatted);
//         it->driver->control((void *)&it->ctx, payload_formatted, &out);
//
//         if (out) {
//           char *outstr = cJSON_PrintUnformatted(out);
//           puts(outstr);
//           cJSON_free(outstr);
//           cJSON_Delete(out);
//         }
//
//         vTaskDelay(pdMS_TO_TICKS(1000));
//         snprintf(payload_formatted, sizeof(payload_formatted), payload_json, 0.0f);
//         ESP_LOGI(TAG, "Sending payload to %s: %s", it->port->port_name, payload_formatted);
//         it->driver->control((void *)&it->ctx, payload_formatted, &out);
//
//         if (out) {
//           char *outstr = cJSON_PrintUnformatted(out);
//           puts(outstr);
//           cJSON_free(outstr);
//           cJSON_Delete(out);
//         }
//       }
//     }
//   }
// }

// void dht_driver_test() {
//   static const sn_port_measurement_map_t dht1_map[] = {
//     MEASUREMENT_MAP_ENTRY(0x01, ST_TEMPERATURE, "C"),
//     MEASUREMENT_MAP_ENTRY(0x02, ST_HUMIDITY, "%"),
//     MEASUREMENT_MAP_ENTRY_NULL(),
//   };
//   // clang-format off
//   static sn_device_port_desc_t ports[] = {
//     SENSOR_PORT_LITERAL("dht-1", "dht11", ((sn_sensor_port_t){
//       .usage.gpio.pin = GPIO_NUM_23,
//       .usage_type = PUT_GPIO,
//       .measurements = dht1_map
//     })),
//   };
//   // clang-format on
//
//   sn_driver_register(&dht_driver);
//   sn_driver_bind_all_ports(ports, sizeof(ports) / sizeof(sn_device_port_desc_t));
//
//   sensor_poll_task(NULL);
// }

// void adc_driver_test() {
//   static const sn_port_measurement_map_t soil1_map[] = {
//     MEASUREMENT_MAP_ENTRY(0x01, ST_MOISTURE, "%"),
//     MEASUREMENT_MAP_ENTRY_NULL(),
//   };
//   static const sn_port_measurement_map_t light1_map[] = {
//     MEASUREMENT_MAP_ENTRY(0x02, ST_LIGHT_INTENSITY, "lux"),
//     MEASUREMENT_MAP_ENTRY_NULL(),
//   };
//   // clang-format off
//   static sn_device_port_desc_t ports[] = {
//     SENSOR_PORT_LITERAL("soil-1", "soil_adc", ((sn_sensor_port_t){
//       .usage.gpio.pin = GPIO_NUM_35,
//       .usage_type = PUT_GPIO,
//       .measurements = soil1_map
//     })),
//     SENSOR_PORT_LITERAL("light-1", "lm393", ((sn_sensor_port_t){
//       .usage.gpio.pin = GPIO_NUM_34,
//       .usage_type = PUT_GPIO,
//       .measurements = light1_map
//     })),
//   };
//   // clang-format on
//
//   sn_driver_register(&soil_moisture_driver);
//   sn_driver_register(&light_intensity_driver);
//   sn_driver_bind_all_ports(ports, sizeof(ports) / sizeof(sn_device_port_desc_t));
//
//   sensor_poll_task(NULL);
// }
