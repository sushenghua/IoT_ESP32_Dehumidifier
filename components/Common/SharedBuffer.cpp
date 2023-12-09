/*
 * SharedBuffer: Wrap shared static buffer
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "SharedBuffer.h"

#define STR_BUFFER_SIZE 1024
static char _strBuf[STR_BUFFER_SIZE];

#define CMD_BUFFER_SIZE 1024
static uint8_t _cmdBuf[CMD_BUFFER_SIZE];

static char _qrStrBuf[STR_BUFFER_SIZE];

char * SharedBuffer::msgBuffer()
{
  return _strBuf;
}

uint8_t * SharedBuffer::cmdBuffer()
{
  return _cmdBuf;
}

char* SharedBuffer::updaterMsgBuffer()
{
  return _qrStrBuf;
}

char * SharedBuffer::qrStrBuffer()
{
  return _qrStrBuf;
}
