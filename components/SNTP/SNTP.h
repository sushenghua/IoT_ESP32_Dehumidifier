/*
 * SNTP: Wrap esp32 c sntp
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _SNTP_H
#define _SNTP_H

#include "esp_system.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"

class SNTP
{
public:
    static void init(uint8_t operationMode = SNTP_OPMODE_POLL);
    static bool inited() { return _inited; }
    static void stop();
    static bool sync(int trials = 10);
    static void setTimezone(const char* zone);
    static time_t& timeNow() {
    	time(&_timeNow);
    	return _timeNow;
    }
    static struct tm& timeInfo(time_t t) {
        localtime_r(&t, &_timeInfo);
        return _timeInfo;
    }
    static void waitSync();
    static void waitSynced();
    static bool synced();

public:
	static void test();

protected:
    static bool         _inited;
    static bool         _synced;
    static time_t       _timeNow;
    static struct tm    _timeInfo;
};

#endif // _SNTP_H
