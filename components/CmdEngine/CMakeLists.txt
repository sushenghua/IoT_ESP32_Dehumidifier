idf_component_register( SRCS "CmdEngine.cpp" "CmdKey.cpp"
                        INCLUDE_DIRS "."
                        REQUIRES MessageProtocol
                        PRIV_REQUIRES json Config Common Sensor Wifi Application AppUpdater )