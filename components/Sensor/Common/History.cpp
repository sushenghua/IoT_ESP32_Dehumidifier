/*
 * history data
 * Copyright (c) 2017 Shenghua Su
 *
 */


#include "History.h"

History::History(uint16_t length)
: _pm2d5MinIndex(0)
, _pm2d5MaxIndex(0)
, _pm10MinIndex(0)
, _pm10MaxIndex(0)
, _hchoMinIndex(0)
, _hchoMaxIndex(0)
, _co2MinIndex(0)
, _co2MaxIndex(0)
, _tempMinIndex(0)
, _tempMaxIndex(0)
, _humidMinIndex(0)
, _humidMaxIndex(0)
, _recentIndex(0)
, _length(length)
, _data(0)
{}


