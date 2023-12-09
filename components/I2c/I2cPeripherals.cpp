/*
 * I2cPeripherals: i2c communication with peripherals
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "I2cPeripherals.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "Semaphore.h"
#include "AppLog.h"

/////////////////////////////////////////////////////////////////////////////////////////
// peripherals power
/////////////////////////////////////////////////////////////////////////////////////////
#define PIN_PERIPHERALS_POWER                   2
#define PERIPHERALS_POWER_ON_LOGIC              0
#define PERIPHERALS_POWER_OFF_LOGIC             1

#define ALLOWED_PERIPHERALS_POWER_RESET_COUNT   1000

#define POWER_FLIP_DELAY                        200
#define POWER_RESET_POST_DELAY                  200
#define I2C_RESET_POST_DELAY                    100

static bool     _i2cPeripheralsPowerOn = true;
static uint16_t _i2cPeripheralsPowerResetCount = 0;
static uint32_t _i2cPeripheralsPowerResetHappenedStamp = 0;

static EventGroupHandle_t _i2cPeripheralsEventGroup;
static const int POWER_RESET_AVAILABLE_BIT = BIT0;

/////////////////////////////////////////////////////////////////////////////////////////
// i2c
/////////////////////////////////////////////////////////////////////////////////////////
#define I2C_PERIPHERALS_PORT                  I2C_NUM_0
#define I2C_PERIPHERALS_PIN_SCK               26
#define I2C_PERIPHERALS_PIN_SDA               27
#define I2C_PERIPHERALS_CLK_SPEED             100000

#define I2C_PERIPHERALS_DEVICE_READY_TRIALS   3

static I2c     *_sharedI2c;
static bool     _i2cPeripheralsInited = false;

void I2cPeripherals::init()
{
  if (!_i2cPeripheralsInited) {
    _i2cPeripheralsInited = true;

    _i2cPeripheralsEventGroup = xEventGroupCreate();
    xEventGroupSetBits(_i2cPeripheralsEventGroup, POWER_RESET_AVAILABLE_BIT); 

    if (xSemaphoreTake(Semaphore::i2c, I2C_MAX_WAIT_TICKS)) {
      // power control init
      gpio_set_direction((gpio_num_t)PIN_PERIPHERALS_POWER, GPIO_MODE_OUTPUT);
      gpio_set_pull_mode((gpio_num_t)PIN_PERIPHERALS_POWER, GPIO_PULLDOWN_ONLY);
      gpio_set_level((gpio_num_t)PIN_PERIPHERALS_POWER, PERIPHERALS_POWER_ON_LOGIC);
      _i2cPeripheralsPowerOn = true;

      // i2c instance init
      _sharedI2c = I2c::instanceForPort(I2C_PERIPHERALS_PORT, I2C_PERIPHERALS_PIN_SCK, I2C_PERIPHERALS_PIN_SDA);
      _sharedI2c->setMode(I2C_MODE_MASTER);
      _sharedI2c->setMasterClkSpeed(I2C_PERIPHERALS_CLK_SPEED);
      _sharedI2c->init();
      xSemaphoreGive(Semaphore::i2c);
    }
  }
}

uint32_t I2cPeripherals::resetStamp()
{
  return _i2cPeripheralsPowerResetHappenedStamp;
}

uint32_t I2cPeripherals::reset()
{
  // wait to obtain reset right
  xEventGroupWaitBits(_i2cPeripheralsEventGroup, POWER_RESET_AVAILABLE_BIT, false, true, portMAX_DELAY);

  // occupy reset right
  xEventGroupClearBits(_i2cPeripheralsEventGroup, POWER_RESET_AVAILABLE_BIT);

  if (xSemaphoreTake(Semaphore::i2c, portMAX_DELAY)) {
    // mark as most recent reset
    ++_i2cPeripheralsPowerResetHappenedStamp;
    APP_LOGW("[I2cPeripherals]", "peripherals reset happened stamp: %ld", _i2cPeripheralsPowerResetHappenedStamp);
    resetPower();
    vTaskDelay( POWER_RESET_POST_DELAY / portTICK_PERIOD_MS );
    _sharedI2c->reset();
    vTaskDelay( I2C_RESET_POST_DELAY / portTICK_PERIOD_MS );
    xSemaphoreGive(Semaphore::i2c);
  }

  // release reset right
  xEventGroupSetBits(_i2cPeripheralsEventGroup, POWER_RESET_AVAILABLE_BIT); 

  return _i2cPeripheralsPowerResetHappenedStamp;
}

/////////////////////////////////////////////////////////////////////////////////////////
// ------ power management
bool I2cPeripherals::powerOn()
{
  return _i2cPeripheralsPowerOn;
}

void I2cPeripherals::setPowerOn(bool on)
{
  if (_i2cPeripheralsPowerOn != on) {
    _i2cPeripheralsPowerOn = on;
    gpio_set_level((gpio_num_t)PIN_PERIPHERALS_POWER, on ? PERIPHERALS_POWER_ON_LOGIC : PERIPHERALS_POWER_OFF_LOGIC);
  }
}

void I2cPeripherals::resetPower()
{
  if (_i2cPeripheralsPowerResetCount >= ALLOWED_PERIPHERALS_POWER_RESET_COUNT) return;

  APP_LOGW("[I2cPeripherals]", "peripherals power reset %d", ++_i2cPeripheralsPowerResetCount);
  gpio_set_level((gpio_num_t)PIN_PERIPHERALS_POWER, PERIPHERALS_POWER_OFF_LOGIC);
  vTaskDelay( POWER_FLIP_DELAY / portTICK_PERIOD_MS );
  gpio_set_level((gpio_num_t)PIN_PERIPHERALS_POWER, PERIPHERALS_POWER_ON_LOGIC);
}

bool I2cPeripherals::resetPowerAllowed()
{
  return _i2cPeripheralsPowerResetCount < ALLOWED_PERIPHERALS_POWER_RESET_COUNT;
}

uint16_t I2cPeripherals::resetPowerCount()
{
  return _i2cPeripheralsPowerResetCount;
}

/////////////////////////////////////////////////////////////////////////////////////////
// ------ communication
void I2cPeripherals::resetI2c(TickType_t semaphoreWaitTicks)
{
  if (xSemaphoreTake(Semaphore::i2c, semaphoreWaitTicks)) {
    _sharedI2c->reset();
    xSemaphoreGive(Semaphore::i2c);
  }
}

bool I2cPeripherals::deviceReady(uint8_t addr, TickType_t semaphoreWaitTicks)
{
  bool ready = false;
  if (xSemaphoreTake(Semaphore::i2c, semaphoreWaitTicks)) {
    for (int i = 0; i < I2C_PERIPHERALS_DEVICE_READY_TRIALS; ++i) {
      if (_sharedI2c->deviceReady(addr)) {
        ready = true;
        break;
      }
    }
    xSemaphoreGive(Semaphore::i2c);
  }
  return ready;
}

bool I2cPeripherals::masterTx(uint8_t addr, uint8_t *data, size_t size, TickType_t semaphoreWaitTicks)
{
  bool ret = false;
  if (xSemaphoreTake(Semaphore::i2c, semaphoreWaitTicks)) {
    ret = _sharedI2c->masterTx(addr, data, size);
    xSemaphoreGive(Semaphore::i2c);
  }
  return ret;
}

bool I2cPeripherals::masterRx(uint8_t addr, uint8_t *data, size_t size, TickType_t semaphoreWaitTicks)
{
  bool ret = false;
  if (xSemaphoreTake(Semaphore::i2c, semaphoreWaitTicks)) {
    ret = _sharedI2c->masterRx(addr, data, size);
    xSemaphoreGive(Semaphore::i2c);
  }
  return ret;
}

bool I2cPeripherals::masterMemTx(uint8_t addr, uint8_t memAddr, uint8_t *data, size_t size, TickType_t semaphoreWaitTicks)
{
  bool ret = false;
  if (xSemaphoreTake(Semaphore::i2c, semaphoreWaitTicks)) {
    ret = _sharedI2c->masterMemTx(addr, memAddr, data, size);
    xSemaphoreGive(Semaphore::i2c);
  }
  return ret;
}

bool I2cPeripherals::masterMemRx(uint8_t addr, uint8_t memAddr, uint8_t *data, size_t size, TickType_t semaphoreWaitTicks)
{
  bool ret = false;
  if (xSemaphoreTake(Semaphore::i2c, semaphoreWaitTicks)) {
    ret = _sharedI2c->masterMemRx(addr, memAddr, data, size);
    xSemaphoreGive(Semaphore::i2c);
  }
  return ret;
}
