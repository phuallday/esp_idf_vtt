idf_component_register(SRCS "vtt_mqtt.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_event esp_netif mqtt)