/*
 * Debug.h Convenient methods
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _DEBUG_H_INCLUDED
#define _DEBUG_H_INCLUDED

#include "Config.h"

/////////////////////////////////////////////////////////////////////////////////////////
// debug print
/////////////////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG_APP

#ifdef __cplusplus
extern "C" {
#endif

void printBuf(const uint8_t *buf, int size);

#ifdef __cplusplus
}
#endif

#endif // DEBUG

#endif // _DEBUG_H_INCLUDED
