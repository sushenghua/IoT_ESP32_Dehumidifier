/*
 * TempHumidData: description of temperature and humidity
 * Copyright (c) 2017 Shenghua Su
 *
 */


#ifndef _TEMP_HUMID_DATA_H
#define _TEMP_HUMID_DATA_H

#include "HealthyStandard.h"

struct TempHumidData {
    // standard from https://www.zhihu.com/question/31515876
    uint8_t         levelTemp;
    uint8_t         levelHumid;

    // tmperature and humidity value
    float           temp;
    float           humid;

    // clear method
    void clear() {
        temp = 0;
        humid = 0;
        levelTemp = 0;
        levelHumid = 0;
    }

    // calculate level
    void calculateLevel() {
        levelTemp = HS::calculateSampleLevel(HS::TEMP_TABLE, temp, TEMP_MAX_LEVEL);
        levelHumid = HS::calculateSampleLevel(HS::HUMID_TABLE, humid, HUMID_MAX_LEVEL);
    }
};

#endif // _TEMP_HUMID_DATA_H
