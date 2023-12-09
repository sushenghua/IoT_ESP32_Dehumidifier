/*
 * I2cPeripherals: i2c communication with peripherals
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _I2C_PERIPHERALS_H
#define _I2C_PERIPHERALS_H

#include "I2c.h"

#define I2C_SEMAPHORE_WAIT_TICKS 1000

class I2cPeripherals
{
public:
  static void init();
  static uint32_t resetStamp();
  static uint32_t reset();

  // --- power management
  static bool powerOn();
  static void setPowerOn(bool on = true);
  static void resetPower();
  static bool resetPowerAllowed();
  static uint16_t resetPowerCount();

  // --- communication
  static void resetI2c(TickType_t semaphoreWaitTicks = I2C_SEMAPHORE_WAIT_TICKS);
  static bool deviceReady(uint8_t addr, TickType_t semaphoreWaitTicks = I2C_SEMAPHORE_WAIT_TICKS);
  static bool masterTx(uint8_t addr, uint8_t *data, size_t size, TickType_t semaphoreWaitTicks = I2C_SEMAPHORE_WAIT_TICKS);
  static bool masterRx(uint8_t addr, uint8_t *data, size_t size, TickType_t semaphoreWaitTicks = I2C_SEMAPHORE_WAIT_TICKS);
  static bool masterMemTx(uint8_t addr, uint8_t memAddr, uint8_t *data, size_t size, TickType_t semaphoreWaitTicks = I2C_SEMAPHORE_WAIT_TICKS);
  static bool masterMemRx(uint8_t addr, uint8_t memAddr, uint8_t *data, size_t size, TickType_t semaphoreWaitTicks = I2C_SEMAPHORE_WAIT_TICKS);
};

#endif // _I2C_PERIPHERALS_H
