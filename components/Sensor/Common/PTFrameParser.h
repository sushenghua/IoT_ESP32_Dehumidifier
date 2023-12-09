/*
 * PTFrameParser Plantower sensor data frame parser
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _PT_FRAME_PARSER_H
#define _PT_FRAME_PARSER_H

#include <stdint.h>

#define BUF_CAPACITY      64

#define FRAME_HEAD1       0x42
#define FRAME_HEAD2       0x4d

typedef enum {
  EXPECT_HEAD1        = 0,
  EXPECT_HEAD2        = 1,
  EXPECT_LENGHT       = 2,
  EXPECT_DATA         = 3,
  EXPECT_CHECKSUM     = 4
} ParseState;

typedef enum {
  FRAME_EMPTY                 = 0,
  FRAME_PARSE_IN_PROGRESS     = 1,
  FRAME_READY                 = 2,
  FRAME_ERR_HEAD_MISMATCH     = 3,
  FRAME_ERR_CHECKSUM_FAILED   = 4
} FrameState;

class PTFrameParser
{
public:
  PTFrameParser()
  : _frameState(FRAME_EMPTY)
  , _parseState(EXPECT_HEAD1)
  , _expectLSB(false)
  , _checksum(0)
  , _dataBlockSize(0)
  , _dataReadCount(0)
  {}

  void reset() {
    _frameState = FRAME_EMPTY;
    _parseState = EXPECT_HEAD1;
    _expectLSB = false;
    _checksum = 0;
    _dataBlockSize = 0;
    _dataReadCount = 0;
  }

  FrameState frameState() { return _frameState; }

  uint16_t dataSize() { return _dataBlockSize; }
  const uint8_t * rawData() { return _buf; }

  uint16_t valueCount() { return _dataBlockSize / 2; }
  uint16_t valueAt(uint16_t index) { return _buf[index * 2] << 8 | _buf[index * 2 + 1]; }

  void parse(uint8_t byte) {
    switch (_parseState) {
      case EXPECT_HEAD1:
        if (byte == FRAME_HEAD1) {
          _frameState = FRAME_PARSE_IN_PROGRESS;
          _parseState = EXPECT_HEAD2;
        }
        break;
      case EXPECT_HEAD2:
        if (byte == FRAME_HEAD2) {
          _parseState = EXPECT_LENGHT;
          _halfWord = 0;
          _checksum = FRAME_HEAD1 + FRAME_HEAD2;
        }
        else {
          _frameState = FRAME_ERR_HEAD_MISMATCH;
          _parseState = EXPECT_HEAD1;
        }
        break;
      case EXPECT_LENGHT:
        if (_expectLSB) {
          _halfWord |= byte;
          _expectLSB = false;
          _dataBlockSize = _halfWord - 2;
          _dataReadCount = 0;
          _parseState = EXPECT_DATA;
        }
        else {
          _halfWord |= byte << 8;
          _expectLSB = true;
        }
        _checksum += byte;
        break;
      case EXPECT_DATA:
        _buf[_dataReadCount++] = byte;
        _checksum += byte;
        if (_dataReadCount >= _dataBlockSize) {
          _parseState = EXPECT_CHECKSUM;
          _halfWord = 0;
        }
        break;
      case EXPECT_CHECKSUM:
        if (_expectLSB) {
          _halfWord |= byte;
          _expectLSB = false;
          if (_checksum == _halfWord) {
            _frameState = FRAME_READY;
          }
          else {
            _frameState = FRAME_ERR_CHECKSUM_FAILED;
          }
          _parseState = EXPECT_HEAD1;
        }
        else {
          _halfWord |= byte << 8;
          _expectLSB = true;
        }
        break;
    }
  }

protected:
  FrameState      _frameState;
  ParseState      _parseState;

  bool            _expectLSB;
  uint16_t        _checksum;
  uint16_t        _halfWord;

  uint16_t        _dataBlockSize;
  uint16_t        _dataReadCount;
  uint8_t         _buf[BUF_CAPACITY];
};

#endif // _PT_FRAME_PARSER_H
