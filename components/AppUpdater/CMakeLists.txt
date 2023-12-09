idf_component_register( SRCS "AppUpdater.cpp" "md5.c"
                        INCLUDE_DIRS "."
                        REQUIRES app_update MessageProtocol
                        PRIV_REQUIRES Config Common Wifi Application )