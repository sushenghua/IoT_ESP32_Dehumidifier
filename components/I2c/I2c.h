/*
 * I2c: Wrap the esp i2c c interface
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _I2C_H
#define _I2C_H

#include <string.h>
#include "driver/i2c.h"

#define I2C_DEFAULT_WAIT_TICKS     1000
#define I2C_MAX_WAIT_TICKS         portMAX_DELAY

class I2c
{
public:
  // static class methods
  static I2c* instanceForPort(i2c_port_t port);
  static I2c* instanceForPort(i2c_port_t port, int pinSck, int pinSda);

public:
  // constructor
  I2c(i2c_port_t port);
  I2c(i2c_port_t port, int pinSck, int pinSda);

  // config, init and deinit
  void setMode(i2c_mode_t mode);
  void setMasterClkSpeed(uint32_t speed);
  void setSlaveAddress(uint16_t addr, uint8_t enable10bitAddr = 0);
  void setPins(int pinSck, int pinSda);
  void init(size_t rxBufLen = 0, size_t txBufLen = 0); // only slave required, master use defualt 0
  void deinit();
  void reset();

  // communication
  bool deviceReady(uint8_t addr, TickType_t waitTicks = I2C_DEFAULT_WAIT_TICKS);
  bool masterTx(uint8_t addr, uint8_t *data, size_t size, TickType_t waitTicks = I2C_DEFAULT_WAIT_TICKS);
  bool masterRx(uint8_t addr, uint8_t *data, size_t size, TickType_t waitTicks = I2C_DEFAULT_WAIT_TICKS);
  bool masterMemTx(uint8_t addr, uint8_t memAddr, uint8_t *data, size_t size, TickType_t waitTicks = I2C_DEFAULT_WAIT_TICKS);
  bool masterMemRx(uint8_t addr, uint8_t memAddr, uint8_t *data, size_t size, TickType_t waitTicks = I2C_DEFAULT_WAIT_TICKS);
  int slaveTx(uint8_t *data, size_t size, TickType_t waitTicks = I2C_DEFAULT_WAIT_TICKS);
  int slaveRx(uint8_t *data, size_t size, TickType_t waitTicks = I2C_DEFAULT_WAIT_TICKS);

protected:
  const i2c_port_t         _port;
  bool                     _inited;
  i2c_config_t             _config;
};

#endif // _I2C_H
