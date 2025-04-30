idf_component_register(SRCS "image.c" "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES ssd1306 esp_wifi nvs_flash wifi_provisioning)
