idf_component_register(SRCS "bsp_can.c" "bsp_dht11.c" "bsp_imu.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp-idf-lib__mpu6050 esp-idf-lib__i2cdev)
