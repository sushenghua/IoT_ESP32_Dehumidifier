/*
 * HealthyStandard: standards for different test parameters
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "HealthyStandard.h"

/////////////////////////////////////////////////////////////////////////////////////////
// Common
/////////////////////////////////////////////////////////////////////////////////////////

uint16_t HS::RGB888toRGB565(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 11) | (g <<5) | b;
}

uint8_t HS::calculateSampleLevel(const float *table, float sampleValue, uint8_t maxLevel)
{
    uint8_t i = 0;
    for (; i < maxLevel; ++i) {
        if (sampleValue < table[i]) {
            break;
        }
    }
    return i;
}

/////////////////////////////////////////////////////////////////////////////////////////
// PM standard
// ref: https://en.wikipedia.org/wiki/Air_quality_index
//      http://www.pm25.com/news/96.html
/////////////////////////////////////////////////////////////////////////////////////////

// table of level index
static uint16_t PMINDEX[] = {
        0,          // HEALTH_LEVEL_GOOD_I_LOW,
        50,         // HEALTH_LEVEL_GOOD_I_HIGH,
        51,         // HEALTH_LEVEL_MODERATE_I_LOW,
        100,        // HEALTH_LEVEL_MODERATE_I_HIGH,
        101,        // HEALTH_LEVEL_UNHEALTHYFORSEN_I_LOW,
        150,        // HEALTH_LEVEL_UNHEALTHYFORSEN_I_HIGH,
        151,        // HEALTH_LEVEL_UNHEALTHY_I_LOW,
        200,        // HEALTH_LEVEL_UNHEALTHY_I_HIGH,
        201,        // HEALTH_LEVEL_VERY_UNHEALTHY_I_LOW,
        300,        // HEALTH_LEVEL_VERY_UNHEALTHY_I_HIGH,
        301,        // HEALTH_LEVEL_HAZARDOUS_I_LOW,
        500,        // HEALTH_LEVEL_HAZARDOUS_I_HIGH
        301,        // same as previous level, sync with breakpoints table
        500         // same as previous level, sync with breakpoints table
};

// table of breakpoints pm2.5, US standard
float HS::PM25US_TABLE[] = {
        0,          // HEALTH_LEVEL_PM25_GOOD_C_LOW,
        12.0f,      // HEALTH_LEVEL_PM25_GOOD_C_HIGH,
        12.1f,      // HEALTH_LEVEL_PM25_MODERATE_C_LOW,
        35.4f,      // HEALTH_LEVEL_PM25_MODERATE_C_HIGH,
        35.5f,      // HEALTH_LEVEL_PM25_UNHEALTHYFORSEN_C_LOW,
        55.4f,      // HEALTH_LEVEL_PM25_UNHEALTHYFORSEN_C_HIGH,
        55.5f,      // HEALTH_LEVEL_PM25_UNHEALTHY_C_LOW,
        150.4f,     // HEALTH_LEVEL_PM25_UNHEALTHY_C_HIGH,
        150.5f,     // HEALTH_LEVEL_PM25_VERY_UNHEALTHY_C_LOW,
        250.4f,     // HEALTH_LEVEL_PM25_VERY_UNHEALTHY_C_HIGH,
        250.5f,     // HEALTH_LEVEL_PM25_HAZARDOUS1_C_LOW,
        350.4f,     // HEALTH_LEVEL_PM25_HAZARDOUS1_C_HIGH,
        350.5f,     // HEALTH_LEVEL_PM25_HAZARDOUS2_C_LOW,
        500.4f      // HEALTH_LEVEL_PM25_HAZARDOUS2_C_HIGH
};

// table of breakpoints pm10, US standard
float HS::PM10US_TABLE[] = {
        0,          // HEALTH_LEVEL_PM10_GOOD_C_LOW,
        54.0f,      // HEALTH_LEVEL_PM10_GOOD_C_HIGH,
        55.0f,      // HEALTH_LEVEL_PM10_MODERATE_C_LOW,
        154.0f,     // HEALTH_LEVEL_PM10_MODERATE_C_HIGH,
        155.0f,     // HEALTH_LEVEL_PM10_UNHEALTHYFORSEN_C_LOW,
        254.0f,     // HEALTH_LEVEL_PM10_UNHEALTHYFORSEN_C_HIGH,
        255.0f,     // HEALTH_LEVEL_PM10_UNHEALTHY_C_LOW,
        354.0f,     // HEALTH_LEVEL_PM10_UNHEALTHY_C_HIGH,
        355.0f,     // HEALTH_LEVEL_PM10_VERY_UNHEALTHY_C_LOW,
        424.0f,     // HEALTH_LEVEL_PM10_VERY_UNHEALTHY_C_HIGH,
        425.0f,     // HEALTH_LEVEL_PM10_HAZARDOUS1_C_LOW,
        504.0f,     // HEALTH_LEVEL_PM10_HAZARDOUS1_C_HIGH,
        505.0f,     // HEALTH_LEVEL_PM10_HAZARDOUS2_C_LOW,
        604.0f      // HEALTH_LEVEL_PM10_HAZARDOUS2_C_HIGH
};

// table of breakpoints pm2.5, China standard
float HS::PM25CN_TABLE[] = {
        0,          // HEALTH_LEVEL_PM25_GOOD_C_LOW,
        35.0f,      // HEALTH_LEVEL_PM25_GOOD_C_HIGH,
        35.1f,      // HEALTH_LEVEL_PM25_MODERATE_C_LOW,
        75.4f,      // HEALTH_LEVEL_PM25_MODERATE_C_HIGH,
        75.5f,      // HEALTH_LEVEL_PM25_UNHEALTHYFORSEN_C_LOW,
        115.4f,     // HEALTH_LEVEL_PM25_UNHEALTHYFORSEN_C_HIGH,
        115.5f,     // HEALTH_LEVEL_PM25_UNHEALTHY_C_LOW,
        150.4f,     // HEALTH_LEVEL_PM25_UNHEALTHY_C_HIGH,
        150.5f,     // HEALTH_LEVEL_PM25_VERY_UNHEALTHY_C_LOW,
        250.4f,     // HEALTH_LEVEL_PM25_VERY_UNHEALTHY_C_HIGH,
        250.5f,     // HEALTH_LEVEL_PM25_HAZARDOUS1_C_LOW,
        350.4f,     // HEALTH_LEVEL_PM25_HAZARDOUS1_C_HIGH,
        350.5f,     // HEALTH_LEVEL_PM25_HAZARDOUS2_C_LOW,
        500.4f      // HEALTH_LEVEL_PM25_HAZARDOUS2_C_HIGH
};

// table of level color
static uint16_t AIRLEVELCOLOR[] = {
        0x07E0,     // good
        0xFFE0,     // moderate
        0xFBE0,     // unhealthy for sensentive groups
        0xF800,     // unhealthy
        0x9809,     // very unhealthy
        0xC804,     // hazardous; original 0x7804 is too dark
        0xC804      // hazardous
};

uint16_t HS::colorForAirLevel(uint8_t level)
{
    return AIRLEVELCOLOR[level];
}

void HS::calculateAQI(const float *table, float concertration, uint16_t &indexValue, uint8_t &level)
{
    int i = 0;
    for (; i <= PM_MAX_LEVEL; ++i) {
        if (concertration <= table[i * 2 + 1]) {
            break;
        }
    }
    if ( i <= PM_MAX_LEVEL) {
    indexValue = ((PMINDEX[i*2+1] - PMINDEX[i*2]) / (table[i*2+1] - table[i*2])) * (concertration - table[i*2])
                  + PMINDEX[i*2];
        level = i;
    }
    else {
        indexValue = 500;
        level = PM_MAX_LEVEL;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
// formaldehyde standard (GB/T50235-2001)
// ref: https://wenku.baidu.com/view/80e13b956bec0975f465e2a1.html
/////////////////////////////////////////////////////////////////////////////////////////

// table of level
float HS::HCHO_TABLE[] = {
        0.02f,      // healthy (excellent)
        0.08f,      // healthy
        0.30f,      // unhealthy
        0.50f,      // very unhealthy
        0.70f       // terribly unhealthy
                    // >0.70f, toxic
};

// table of level color
static uint16_t HCHOLEVELCOLOR[] = {
        0x07E0,     // excellent (same color as 'good')
        0x07E0,     // good
        0xFBE0,     // unhealthy
        0xF800,     // very unhealthy
        0x9809,     // terribly unhealthy
        0xC804      // toxic; original 0x7804 is too dark
};

uint16_t HS::colorForHchoLevel(uint8_t level)
{
    return HCHOLEVELCOLOR[level];
}

/////////////////////////////////////////////////////////////////////////////////////////
// temperature and humidity standard
// ref: https://www.zhihu.com/question/31515876
/////////////////////////////////////////////////////////////////////////////////////////

// temperature level
float HS::TEMP_TABLE[] = {
       -10.0f,      // deep cold
         5.0f,      // icy cold
        18.0f,      // cold
        27.0f,      // comfortable
        32.0f,      // hot
        38.0f       // very hot
                    // burn hot
};

// table of level color
static uint16_t TEMPLEVELCOLOR[] = {
        0x07FF,     // deep cold
        0x067F,     // icy cold
        0x04DF,     // cold
        0x07E0,     // comfortable
        0xFBE0,     // hot
        0xF800,     // very hot
        0xC804      // burn hot; original 0x7804 is too dark
};

uint16_t HS::colorForTempLevel(uint8_t level)
{
    return TEMPLEVELCOLOR[level];
}

// humidity level
float HS::HUMID_TABLE[] = {
        40.0f,      // dry
        70.0f       // comfortable
                    // humid
};

// table of level color
static uint16_t HUMIDLEVELCOLOR[] = {
        0xFBE0,     // dry
        0x07E0,     // comfortable
        0x981F      // humid
};

uint16_t HS::colorForHumidLevel(uint8_t level)
{
    return HUMIDLEVELCOLOR[level];
}

/////////////////////////////////////////////////////////////////////////////////////////
// luminosity standard
// ref:
/////////////////////////////////////////////////////////////////////////////////////////

// luminosity level
float HS::LUMI_TABLE[] = {
        100.0f,     // dark
        1000.0f     // comfortable
                    // strong
};

// table of level color
static uint16_t LUMILEVELCOLOR[] = {
        0xFBE0,     // dark
        0x07E0,     // comfortable
        0x07FF      // strong
};

uint16_t HS::colorForLumiLevel(uint8_t level)
{
    return LUMILEVELCOLOR[level];
}

/////////////////////////////////////////////////////////////////////////////////////////
// CO2 standard
// ref: https://zhidao.baidu.com/question/475675906.html
/////////////////////////////////////////////////////////////////////////////////////////

// CO2 level
float HS::CO2_TABLE[] = {
         450.0f,    // outside fresh
        1000.0f,    // fresh
        2000.0f,    // stagnant, sleepy
        5000.0f,    // uncomfortable, heart-beat accelerated
                    // dagerous, coma, event dead
};

// table of level color
static uint16_t CO2LEVELCOLOR[] = {
        0x07F9,     // outside fresh
        0x07E0,     // fresh
        0xFBE0,     // stagnant, sleepy
        0xF800,     // uncomfortable, heart-beat accelerated
        0xC804      // dagerous, coma, event dead; original 0x7804 is too dark
};

uint16_t HS::colorForCO2Level(uint8_t level)
{
    return CO2LEVELCOLOR[level];
}
