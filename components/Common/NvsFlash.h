/*
 * NvsFlash: Wrap nvs flash
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _NVS_FLASH_H
#define _NVS_FLASH_H

#include <stddef.h>

class NvsFlash
{
public:
  static void init();
  static bool inited() { return _inited; }
  static bool loadData(const char *STORAGE_TAG, void *out, size_t loadSize,
                       const char *SAVE_COUNT_TAG = NULL);
  static bool saveData(const char *STORAGE_TAG, const void *data, size_t saveSize,
                       const char *SAVE_COUNT_TAG = NULL);

protected:
  static bool _inited;
};

#endif // _NVS_FLASH_H
