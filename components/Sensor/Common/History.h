/*
 * history data
 * Copyright (c) 2017 Shenghua Su
 *
 */


#ifndef _HISTORY_H
#define _HISTORY_H

#include "SensorConfig.h"


/////////////////////////////////////////////////////////////////////////////////////////
// History data
/////////////////////////////////////////////////////////////////////////////////////////
struct PMItem {
  uint8_t         levelPm2d5US;
  uint8_t         levelPm10US;
  uint16_t        aqiPm2d5US;
  uint16_t        aqiPm10US;
  uint16_t        colorPm2d5US;
  uint16_t        colorPm10US;

  float           pm1d0;
  float           pm2d5;
  float           pm10;
};

struct GeneralItem {
  uint8_t         level;
  uint16_t        color;
  float           value;

  void clear() {
      level = 0;
      color = 0;
      value = 0;
  }
};

struct HistoryData {
  PMItem            pm;
  GeneralItem       hcho;
  GeneralItem       temp;
  GeneralItem       humid;
  GeneralItem       co2;
};


/////////////////////////////////////////////////////////////////////////////////////////
// History class
/////////////////////////////////////////////////////////////////////////////////////////
#define HISTORY_DEFAULT_LENGTH  60*24  // 1/min x 60/h x 24/d
class History {
public:
  // constructor
  History(uint16_t length = HISTORY_DEFAULT_LENGTH);

protected:
  // PM min max index
  uint16_t        _pm2d5MinIndex;
  uint16_t        _pm2d5MaxIndex;
  uint16_t        _pm10MinIndex;
  uint16_t        _pm10MaxIndex;

  // HCHO min max index
  uint16_t        _hchoMinIndex;
  uint16_t        _hchoMaxIndex;

  // CO2 min max index
  uint16_t        _co2MinIndex;
  uint16_t        _co2MaxIndex;

  // temp min max index
  uint16_t        _tempMinIndex;
  uint16_t        _tempMaxIndex;

  // humid min max index
  uint16_t        _humidMinIndex;
  uint16_t        _humidMaxIndex;

  // history data recent index
  uint16_t        _recentIndex;

  // history data length
  uint16_t        _length;

  // history data block
  HistoryData    *_data;
};

#endif // _HISTORY_H
