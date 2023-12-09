idf_component_register( SRCS "Debug.cpp" "NvsFlash.cpp" "Semaphore.cpp" "SharedBuffer.cpp"
                        INCLUDE_DIRS "."
                        REQUIRES Config
                        PRIV_REQUIRES nvs_flash )