/*
 * SensorDataPacker: for package different sensors' data into single block
 * Copyright (c) 2016 Shenghua Su
 *
 */

#ifndef _SENSOR_DATA_PACKER_H
#define _SENSOR_DATA_PACKER_H

#include <stdint.h>
#include <stddef.h>
#include "SHT3xSensor.h"
#include "DehumidFan.h"

#define BUF_SIZE (sizeof(TempHumidData))

class SensorDataPacker
{
public:
    // shared instance
    static SensorDataPacker * sharedInstance();

    // init
    void init();

    // add sensor obj ref
    void setTempHumidSensor(SHT3xSensor *sensor0, SHT3xSensor *sensor1=NULL);
    void setDehumidFan(DehumidFan *dehumidFan);

    // sensor capability
    uint32_t sensorCapability() { return _sensorCapability; }

    // get data
    const uint8_t * dataBlock(size_t &size);
    const char*     dataJsonString(size_t &size);

public:
    SensorDataPacker();

protected:
    bool                 _inited;
    SHT3xSensor         *_thSensor0;
    SHT3xSensor         *_thSensor1;
    DehumidFan          *_dehumidFan;
    uint32_t             _sensorCapability;
    uint8_t              _dataBlockBuf[BUF_SIZE];
};

#endif // _SENSOR_DATA_PACKER_H
