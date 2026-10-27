idf_component_register(SRCS "bme280.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_driver_gpio esp_driver_i2c
                       PRIV_REQUIRES log)
