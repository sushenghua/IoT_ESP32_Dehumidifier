/*
 * Debug.h Convenient methods
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "Debug.h"

#ifdef DEBUG_APP

#include <rom/ets_sys.h>

void printBuf(const uint8_t *buf, int size)
{
	ets_printf("[");
	for (int i = 0; i < size; ++i) {
		ets_printf("%d, ", buf[i]);
	}
	ets_printf("]\n");
}

#endif // DEBUG_APP
