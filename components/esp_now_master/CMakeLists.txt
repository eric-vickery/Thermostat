idf_component_register(SRCS "esp_now_master.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_wifi nvs_flash esp_timer)
