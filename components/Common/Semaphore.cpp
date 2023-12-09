/*
 * Semaphore: predefined static semaphore
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "Semaphore.h"

static bool _inited = false;

SemaphoreHandle_t Semaphore::i2c = 0;

void Semaphore::init()
{
  if (!_inited) {
  	i2c = xSemaphoreCreateMutex();
  	_inited = true;
  }
}

void Semaphore::deinit()
{
  if (_inited) {
  	vSemaphoreDelete(i2c);
  	_inited = false;
  }
}
