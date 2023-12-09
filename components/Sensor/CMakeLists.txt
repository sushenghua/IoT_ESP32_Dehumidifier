idf_component_register( SRC_DIRS "Common" "TempHumid" "SensorDataPacker" "DehumidFan"
                        INCLUDE_DIRS "Common" "TempHumid" "SensorDataPacker" "DehumidFan"
                        REQUIRES
                        PRIV_REQUIRES driver Config Common I2c Application )