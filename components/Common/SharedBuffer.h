/*
 * SharedBuffer: Wrap shared static buffer
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _SHARED_BUFFER_H
#define _SHARED_BUFFER_H

#include <stdint.h>

class SharedBuffer
{
public:
  static char*     msgBuffer();
  static uint8_t * cmdBuffer();
  static char*     updaterMsgBuffer();
  static char*     qrStrBuffer();
};

#endif // _SHARED_BUFFER_H
