idf_component_register( SRCS "System.cpp"
                        INCLUDE_DIRS "."
                        REQUIRES
                        PRIV_REQUIRES esp_timer app_update esp_partition spi_flash esp_hw_support
                                      bootloader_support esp_system Config Common I2c Wifi SNTP
                                      MessageProtocol CmdEngine AppUpdater Sensor)