/*
 * AppLog.h wrap esp log
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _APP_LOG_H
#define _APP_LOG_H

#define ENABLE_APP_LOG
#ifdef ENABLE_APP_LOG

#include "esp_log.h"
#include <stdio.h>

#define LOG_COLOR_C       LOG_COLOR(LOG_COLOR_CYAN)
#define APP_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " (%ld) %s: " format LOG_RESET_COLOR "\n"

// #define APP_LOGE( tag, format, ... )  ESP_LOGE(tag, format, ##__VA_ARGS__)
// #define APP_LOGW( tag, format, ... )  ESP_LOGW(tag, format, ##__VA_ARGS__)
// #define APP_LOGI( tag, format, ... )  ESP_LOGI(tag, format, ##__VA_ARGS__)
// #define APP_LOGD( tag, format, ... )  ESP_LOGD(tag, format, ##__VA_ARGS__)
// #define APP_LOGV( tag, format, ... )  ESP_LOGV(tag, format, ##__VA_ARGS__)

#define APP_LOGE( tag, format, ... )  printf(APP_LOG_FORMAT(E, format), esp_log_timestamp(), tag, ##__VA_ARGS__)
#define APP_LOGW( tag, format, ... )  printf(APP_LOG_FORMAT(W, format), esp_log_timestamp(), tag, ##__VA_ARGS__)
#define APP_LOGI( tag, format, ... )  printf(APP_LOG_FORMAT(I, format), esp_log_timestamp(), tag, ##__VA_ARGS__)
#define APP_LOGD( tag, format, ... )  printf(APP_LOG_FORMAT(D, format), esp_log_timestamp(), tag, ##__VA_ARGS__)
#define APP_LOGV( tag, format, ... )  printf(APP_LOG_FORMAT(V, format), esp_log_timestamp(), tag, ##__VA_ARGS__)
#define APP_LOGC( tag, format, ... )  printf(APP_LOG_FORMAT(C, format), esp_log_timestamp(), tag, ##__VA_ARGS__)

#else

#define APP_LOGE( tag, format, ... )
#define APP_LOGW( tag, format, ... )
#define APP_LOGI( tag, format, ... )
#define APP_LOGD( tag, format, ... )
#define APP_LOGV( tag, format, ... )
#define APP_LOGC( tag, format, ... )

#endif // ENABLE_APP_LOG

#endif // _APP_LOG_H