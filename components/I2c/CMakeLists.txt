idf_component_register( SRCS "I2cPeripherals.cpp" "I2c.cpp"
                        INCLUDE_DIRS "."
                        REQUIRES driver
                        PRIV_REQUIRES Common )