idf_component_register( SRCS "Wifi.cpp"
                        INCLUDE_DIRS "."
                        REQUIRES esp_wifi
                        PRIV_REQUIRES  Common Application wpa_supplicant )