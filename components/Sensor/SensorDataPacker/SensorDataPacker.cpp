/*
 * SensorDataPacker: for package different sensors' data into single block
 * Copyright (c) 2016 Shenghua Su
 *
 */

#include "SensorDataPacker.h"
#include <string.h>
#include <stdio.h>
#include "System.h"
#include "SharedBuffer.h"

static SensorDataPacker _sharedSensorDataPacker;

SensorDataPacker * SensorDataPacker::sharedInstance()
{
  return &_sharedSensorDataPacker;
}

char  *_dataStringBuf = NULL;

SensorDataPacker::SensorDataPacker()
: _inited(false)
, _thSensor0(NULL)
, _thSensor1(NULL)
, _dehumidFan(NULL)
{}

void SensorDataPacker::init()
{
  if (!_inited) {
    _dataStringBuf = SharedBuffer::msgBuffer();
    _inited = true;
  }
}

void SensorDataPacker::setTempHumidSensor(SHT3xSensor *sensor0, SHT3xSensor *sensor1)
{
  _thSensor0 = sensor0;
  _thSensor1 = sensor1;
}

void SensorDataPacker::setDehumidFan(DehumidFan *dehumidFan)
{
  _dehumidFan = dehumidFan;
}

const uint8_t* SensorDataPacker::dataBlock(size_t &size)
{
  size_t packCount = 0;
  size_t sz;

  if (_thSensor0) {
    sz = sizeof(_thSensor0->tempHumidData());
    memcpy(_dataBlockBuf + packCount, &(_thSensor0->tempHumidData()), sz);
    packCount += sz;
  }

  if (_thSensor1) {
    sz = sizeof(_thSensor1->tempHumidData());
    memcpy(_dataBlockBuf + packCount, &(_thSensor1->tempHumidData()), sz);
    packCount += sz;
  }

  if (_dehumidFan) {
    uint8_t fansOn[2];
    fansOn[1] = _dehumidFan->fan1On();
    fansOn[2] = _dehumidFan->fan2On();
    sz = sizeof(fansOn);
    memcpy(_dataBlockBuf + packCount, fansOn, sz);
    packCount += sz;
  }

  size = packCount;
  return _dataBlockBuf;
}

const char* SensorDataPacker::dataJsonString(size_t &size)
{
  size_t packCount = 0;
  bool commaPreceded = false;

  sprintf(_dataStringBuf + packCount, "{\"ret\":{");
  packCount += strlen(_dataStringBuf + packCount);

  if (_thSensor0) {
    TempHumidData th = _thSensor0->tempHumidData();
    sprintf(_dataStringBuf + packCount,
            "\"s0\":{\"temp\":%.1f,\"humid\":%.1f}",
            th.temp, th.humid);
    packCount += strlen(_dataStringBuf + packCount);
    commaPreceded = true;
  }

  if (_thSensor1) {
    TempHumidData th = _thSensor1->tempHumidData();
    sprintf(_dataStringBuf + packCount,
            "%s\"s1\":{\"temp\":%.1f,\"humid\":%.1f}",
            commaPreceded ? "," : "",
            th.temp, th.humid);
    packCount += strlen(_dataStringBuf + packCount);
    commaPreceded = true;
  }

  if (_dehumidFan) {
    sprintf(_dataStringBuf + packCount,
            "%s\"f0\":{\"on\":%s},\"f1\":{\"on\":%s}",
            commaPreceded ? "," : "",
            _dehumidFan->fan1On()? "true" : "false",
            _dehumidFan->fan2On()? "true" : "false");
    packCount += strlen(_dataStringBuf + packCount);
    commaPreceded = true;
  }

  sprintf(_dataStringBuf + packCount, "}}");
  packCount += strlen(_dataStringBuf + packCount);

  size = packCount;
  return _dataStringBuf;
}
