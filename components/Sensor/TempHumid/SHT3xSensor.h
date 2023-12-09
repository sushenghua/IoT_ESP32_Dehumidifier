/*
 * SHT3xSensor Wrap communicate with SHT3xSensor temperature and humidity sensor
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _SHT3X_SENSOR_H
#define _SHT3X_SENSOR_H

#include "TempHumidData.h"

#define PM_RX_BUF_CAPACITY      PM_RX_PROTOCOL_MAX_LENGTH

class SHT3xSensor
{
public:
  // constructor
  SHT3xSensor(uint8_t index = 0);

  // init
  void init(bool checkDeviceReady=true);

  // cached values
  TempHumidData & tempHumidData() { return _tempHumidData; }

  // sample
  void sampleData();
  bool sampleValid() { return _sampleValid; }

protected:
  // sensor address
  uint8_t         _addr;
  // indicate if recent sample valid
  bool            _sampleValid;
  // value cache from sensor
  TempHumidData   _tempHumidData;
};

#endif // _SHT3X_SENSOR_H
