/*
 * DehumidFan class
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "DehumidFan.h"
#include "driver/gpio.h"
// #include "AppLog.h"

/////////////////////////////////////////////////////////////////////////////////////////
// fan power
/////////////////////////////////////////////////////////////////////////////////////////
#define FAN1_PWR_PIN                  19
#define FAN2_PWR_PIN                  23

#define FAN_POWER_ON_LOGIC            1
#define FAN_POWER_OFF_LOGIC           0

static bool   _humidControlInited = false;
static bool   _fan1On = false;
static bool   _fan2On = false;


/////////////////////////////////////////////////////////////////////////////////////////
// DehumidFan class
/////////////////////////////////////////////////////////////////////////////////////////
void DehumidFan::init()
{
  if (!_humidControlInited) {

    gpio_set_direction((gpio_num_t)FAN1_PWR_PIN, GPIO_MODE_OUTPUT);
    // gpio_set_pull_mode((gpio_num_t)FAN1_PWR_PIN, GPIO_PULLDOWN_ONLY);
    gpio_set_level((gpio_num_t)FAN1_PWR_PIN, FAN_POWER_OFF_LOGIC);

    gpio_set_direction((gpio_num_t)FAN2_PWR_PIN, GPIO_MODE_OUTPUT);
    // gpio_set_pull_mode((gpio_num_t)FAN2_PWR_PIN, GPIO_PULLDOWN_ONLY);
    gpio_set_level((gpio_num_t)FAN2_PWR_PIN, FAN_POWER_OFF_LOGIC);

    _humidControlInited = true;
  }
}

bool DehumidFan::fan1On()
{
  return _fan1On;
}

bool DehumidFan::fan2On()
{
  return _fan2On;
}

void DehumidFan::setFan1On(bool on)
{
  if (_fan1On != on) {
    _fan1On = on;
    gpio_set_level((gpio_num_t)FAN1_PWR_PIN, on ? FAN_POWER_ON_LOGIC : FAN_POWER_OFF_LOGIC);
  }
}

void DehumidFan::setFan2On(bool on)
{
  if (_fan2On != on) {
    _fan2On = on;
    gpio_set_level((gpio_num_t)FAN2_PWR_PIN, on ? FAN_POWER_ON_LOGIC : FAN_POWER_OFF_LOGIC);
  }
}
