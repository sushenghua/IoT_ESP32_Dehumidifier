/*
 * sensor data pack
 * Copyright (c) 2017 Shenghua Su
 *
 */


#ifndef _SENSOR_DATA_H
#define _SENSOR_DATA_H

struct SensorData {
  // co2 display
  uint8_t     co2Level;
  uint16_t    co2Color;
  float       co2;

  // pm display
  uint8_t     pm2d5Level;
  uint8_t     pm10Level;
  uint16_t    pm2d5Color;
  uint16_t    pm10Color;
  uint16_t    aqiPm2d5US;
  uint16_t    aqiPm10US;
  float       pm1d0;
  float       pm2d5;
  float       pm10;

  // formaldehyde display
  uint8_t     hchoLevel;
  uint16_t    hchoColor;
  float       hcho;

  // temperature and humidity diplay
  uint8_t     tempLevel;
  uint8_t     humidLevel;
  uint16_t    tempColor;
  uint16_t    humidColor;
  float       temp;
  float       humid;

  // luminosity
  uint8_t     lumiLevel;
  uint16_t    lumiColor;
  uint32_t    luminosity;
};

#endif
