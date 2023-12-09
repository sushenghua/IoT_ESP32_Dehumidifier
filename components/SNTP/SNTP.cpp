/*
 * SNTP: Wrap esp32 c sntp
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "SNTP.h"
#include "AppLog.h"
#include "Wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

bool       SNTP::_inited = false;
bool       SNTP::_synced = false;
time_t     SNTP::_timeNow = 0;
struct tm  SNTP::_timeInfo;

// FreeRTOS event group to signal when we time get synced
static EventGroupHandle_t sntpEventGroup;
static const int SYNCED_BIT = BIT0;

void SNTP::init(uint8_t operationMode) {
    if (!_inited) {
        APP_LOGI("[SNTP]", "init SNTP");
        sntpEventGroup = xEventGroupCreate();
        Wifi::instance()->waitConnected(); // block wait
        sntp_setoperatingmode(operationMode);
        // sntp_setservername(0, (char*)("pool.ntp.org"));
        sntp_setservername(0, (char*)("cn.pool.ntp.org"));
        sntp_init();
        _inited = true;
    }
}

void SNTP::stop()
{
    if (_inited) {
        sntp_stop();
    }
}

void SNTP::setTimezone(const char* zone)
{
    setenv("TZ", zone, 1);
    tzset();
}

bool SNTP::sync(int trials)
{
    // wait for time to be set
    _timeNow = 0;
    memset(&_timeInfo, 0, sizeof(_timeInfo));
    int retry = 0;
    while(_timeInfo.tm_year < (2016 - 1900) && ++retry <= trials) {
        //APP_LOGI("[SNTP]", "waiting for system time to be set... (%d/%d)", retry, trials);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&_timeNow);
        localtime_r(&_timeNow, &_timeInfo);
    }
    _synced = _timeInfo.tm_year > (2016 - 1900);
    return _synced;
}

void SNTP::waitSync()
{
    // block wait wifi connected and sync successfully
    APP_LOGI("[SNTP]", "waiting time sync ...");
    while (!SNTP::sync()) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    xEventGroupSetBits(sntpEventGroup, SYNCED_BIT);
}

void SNTP::waitSynced()
{
    xEventGroupWaitBits(sntpEventGroup, SYNCED_BIT, false, true, portMAX_DELAY);
}

bool SNTP::synced()
{
    return _synced;
}

void SNTP::test()
{
    time_t now;
    char strftime_buf[64];

    now = timeNow();
    // Set timezone to Eastern Standard Time and print local time
    setTimezone("EST5EDT,M3.2.0/2,M11.1.0");
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeInfo(now));
    APP_LOGI("[SNTP]", "The current date/time in New York is: %s", strftime_buf);

    // Set timezone to China Standard Time
    setTimezone("CST-8CDT-8,M4.2.0/2,M9.2.0/3");
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeInfo(now));
    APP_LOGI("[SNTP]", "The current date/time in Shanghai is: %s", strftime_buf);
}
