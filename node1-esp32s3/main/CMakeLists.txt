idf_component_register(SRCS "main.c" "task_structure.c" "task_env.c" "capture.c"
                    INCLUDE_DIRS "."
                    REQUIRES service bsp esp-idf-lib__dht esp_timer)
