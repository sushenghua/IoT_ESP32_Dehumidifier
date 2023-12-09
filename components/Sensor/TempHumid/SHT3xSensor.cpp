/*
 * SHT3xSensor Wrap communicate with Temperature Humidity sensor
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "SHT3xSensor.h"
#include "Config.h"
#include "AppLog.h"
#include "I2c.h"
#include "Semaphore.h"
#include "I2cPeripherals.h"
#include "System.h"

/////////////////////////////////////////////////////////////////////////////////////////
// SHT3X Temperature Humidity sensor I2C
/////////////////////////////////////////////////////////////////////////////////////////
#define SHT3X_ADDR_1                         0x44
#define SHT3X_ADDR_2                         0x45

#define SHT3X_I2C_SEMAPHORE_WAIT_TICKS       1000

bool sht3xReady(uint8_t addr)
{
  return I2cPeripherals::deviceReady(addr);
}

/////////////////////////////////////////////////////////////////////////////////////////
// SHT3X comamnd, copied from Adafruit_SHT31.cpp
/////////////////////////////////////////////////////////////////////////////////////////

#define SHT3X_MEAS_HIGHREP_STRETCH     0x2C06
#define SHT3X_MEAS_MEDREP_STRETCH      0x2C0D
#define SHT3X_MEAS_LOWREP_STRETCH      0x2C10
#define SHT3X_MEAS_HIGHREP             0x2400
#define SHT3X_MEAS_MEDREP              0x240B
#define SHT3X_MEAS_LOWREP              0x2416
#define SHT3X_READSTATUS               0xF32D
#define SHT3X_CLEARSTATUS              0x3041
#define SHT3X_SOFTRESET                0x30A2
#define SHT3X_HEATEREN                 0x306D
#define SHT3X_HEATERDIS                0x3066

#ifndef delay
#define delay(x)                 vTaskDelay((x)/portTICK_PERIOD_MS)
#endif

uint8_t _sht3xTxBuf[2];

void sht3xSendCmd(uint8_t addr, uint16_t cmd)
{
  _sht3xTxBuf[0] = cmd >> 8;
  _sht3xTxBuf[1] = cmd & 0xFF;
  I2cPeripherals::masterTx(addr, _sht3xTxBuf, 2);
}

bool sht3xRxData(uint8_t addr, uint8_t *buf, size_t count, TickType_t waitTicks = I2C_DEFAULT_WAIT_TICKS)
{
  return I2cPeripherals::masterRx(addr, buf, count, waitTicks);
}

void sht3xReset(uint8_t addr)
{
  sht3xSendCmd(addr, SHT3X_SOFTRESET);
  delay(10);
}

void sht3xEnableHeater(uint8_t addr, bool enabled)
{
  sht3xSendCmd(addr, enabled ? SHT3X_HEATEREN : SHT3X_HEATERDIS);
}

const uint8_t CRC_POLYNOMIAL(0x31);

uint8_t sht3xCRC8(const uint8_t *data, int len)
{
 /*
  * CRC-8 formula from section 4.11 of SHT spec pdf
  *
  * Test data 0xBE, 0xEF should yield 0x92
  *
  * Initialization data 0xFF
  * Polynomial 0x31 (x8 + x5 +x4 +1)
  * Final XOR 0x00
  */
  uint8_t crc(0xFF);
  for ( int j = len; j; --j ) {
    crc ^= *data++;
    for ( int i = 8; i; --i ) {
      crc = ( crc & 0x80 ) ? (crc << 1) ^ CRC_POLYNOMIAL : (crc << 1);
    }
  }
  return crc;
}

uint8_t _sht3xRxBuf[6];

bool sht3xReadTempHumid(uint8_t addr, float &temperature, float &humidity)
{
  sht3xSendCmd(addr, SHT3X_MEAS_HIGHREP);

  delay(400);

  bool ok = true;
  uint16_t temp, humid;

  do {

    ok = sht3xRxData(addr, _sht3xRxBuf, 6);
    if (!ok) break;

    temp = _sht3xRxBuf[0]; temp <<= 8;
    temp |= _sht3xRxBuf[1];

    if (_sht3xRxBuf[2] != sht3xCRC8(_sht3xRxBuf, 2)) {
      ok = false; break;
    }

    humid = _sht3xRxBuf[3]; humid <<= 8;
    humid |= _sht3xRxBuf[4];

    if (_sht3xRxBuf[5] != sht3xCRC8(_sht3xRxBuf + 3, 2)) {
      ok = false; break;
    }

    temperature = 175.0 * temp / 0xFFFF - 45;
    humidity = 100.0 * humid / 0xFFFF;

  } while (false);

  return ok;
}

bool sht3xReadStatus(uint8_t addr, uint16_t &status)
{
  sht3xSendCmd(addr, SHT3X_READSTATUS);
  // delay(5);
  if (sht3xRxData(addr, _sht3xRxBuf, 3)) {
    status = _sht3xRxBuf[0]; status <<= 8;
    status |= _sht3xRxBuf[1];
    return true;
  }
  return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
// SHT3xSensor class
/////////////////////////////////////////////////////////////////////////////////////////
SHT3xSensor::SHT3xSensor(uint8_t index)
: _sampleValid(false)
{
  if (index > 1) {
    APP_LOGE("[SHT3X]", "SHT3X sensor index error, valid value: 0 or 1");
  }
  _addr = SHT3X_ADDR_1 + index;
  _tempHumidData.clear();
}

#define  SHT3X_TRY_INIT_DELAY             100
#define  SHT3X_RESET_ON_SAMPLE_FAIL_COUNT 1
uint16_t _sht3xSampleFailCount = 0;
uint32_t _sht3xPwrResetStamp = 0;

void SHT3xSensor::init(bool checkDeviceReady)
{
  APP_LOGI("[SHT3X]", "SHT3X sensor init");

  // check ready, it blocks task here if not ready
  while (checkDeviceReady && !sht3xReady(_addr)) {
    APP_LOGE("[SHT3X]", "SHT3X sensor not found");
    if (_sht3xPwrResetStamp == I2cPeripherals::resetStamp()) {
      APP_LOGC("[SHT3X]", "SHT3X sensor require reset");
      _sht3xPwrResetStamp = I2cPeripherals::reset();
    }
    delay(SHT3X_TRY_INIT_DELAY);
  }
  // sync power reset count
  _sht3xPwrResetStamp = I2cPeripherals::resetStamp();
  // reset sample fail count
  _sht3xSampleFailCount = 0;
  // soft-reset the sensor
  sht3xReset(_addr);
}

void SHT3xSensor::sampleData()
{
  // other tasks has reset the i2c peripherals power
  if (_sht3xPwrResetStamp < I2cPeripherals::resetStamp()) {
    APP_LOGE("[SHT3X]", "reset stamp out of sync");
    init(false);
  }

  _sampleValid = sht3xReadTempHumid(_addr, _tempHumidData.temp, _tempHumidData.humid);
  if (_sampleValid) {
    // APP_LOGC("[SHT3X]", "[%d]: --->temp: %2.2f  humid: %2.2f", 
    //                     _addr - SHT3X_ADDR_1, _tempHumidData.temp, _tempHumidData.humid);
#ifdef DEBUG_APP_OK
    APP_LOGC("[SHT3X]", "--->temp: %2.2f  humid: %2.2f", _tempHumidData.temp, _tempHumidData.humid);
#endif
  }
  else {
#ifdef DEBUG_APP_ERR
    APP_LOGE("[SHT3X]", "sample data failed");
#endif
    if (++_sht3xSampleFailCount == SHT3X_RESET_ON_SAMPLE_FAIL_COUNT) init();
  }
}
