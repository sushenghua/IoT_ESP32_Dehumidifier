idf_component_register( SRC_DIRS "."
                        INCLUDE_DIRS "."
                        REQUIRES esp_https_server esp_websocket_client mqtt
                        PRIV_REQUIRES mbedtls esp_timer esp_websocket_client
                                      Config Common Wifi Application SNTP
                        EMBED_TXTFILES "certs/servercert.pem"
                                       "certs/prvtkey.pem" 
                                       "html/index.html")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable)
