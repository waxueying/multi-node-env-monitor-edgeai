idf_component_register(SRCS "wifi_ota.c" "can_protocol.c" "structure_monitor.c" "environment_monitor.c" "ai_infer.cpp" "node_health.c"
                    INCLUDE_DIRS "include"
                    REQUIRES bsp pal esp_timer espressif__esp-tflite-micro
                             esp_wifi esp_netif esp_http_client esp-tls json nvs_flash app_update)

