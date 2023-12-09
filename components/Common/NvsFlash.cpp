/*
 * NvsFlash: Wrap nvs flash
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "NvsFlash.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "AppLog.h"

bool NvsFlash::_inited = false;

void NvsFlash::init()
{
  if (!_inited) {
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
      // OTA app partition table has a smaller NVS partition size than the non-OTA
      // partition table. This size mismatch may cause NVS initialization to fail.
      // If this happens, we erase NVS partition and initialize NVS again.
      const esp_partition_t* nvs_partition = esp_partition_find_first(
          ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
      assert(nvs_partition && "partition table must have an NVS partition");
      ESP_ERROR_CHECK( esp_partition_erase_range(nvs_partition, 0, nvs_partition->size) );
      err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    _inited = (err == ESP_OK);   
  }
}

#define APP_STORAGE_NAMESPACE  "app"

bool NvsFlash::loadData(const char *STORAGE_TAG, void *out, size_t loadSize, const char *SAVE_COUNT_TAG)
{
  bool succeeded = false;
  bool nvsOpened = false;

  nvs_handle nvsHandle;
  esp_err_t err;

  do {
    // open nvs
    err = nvs_open(APP_STORAGE_NAMESPACE, NVS_READONLY, &nvsHandle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
      APP_LOGE("[NvsFlash]", "loadData open nvs: storage \"%s\" not found", APP_STORAGE_NAMESPACE);
      break;
    }
    else if (err != ESP_OK) {
      APP_LOGE("[NvsFlash]", "loadData open nvs failed %d", err);
      break;
    }
    nvsOpened = true;

    uint16_t saveCount = 1; // enable data read if no SAVE_COUNT_TAG given
    // save count check
    if (SAVE_COUNT_TAG) {
      saveCount = 0; // value will default to 0, if not set yet in NVS
      err = nvs_get_u16(nvsHandle, SAVE_COUNT_TAG, &saveCount);
      if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        APP_LOGE("[NvsFlash]", "loadData read save-count \"%s\" failed %d", SAVE_COUNT_TAG, err);
        break;
      }
    }

    // read data
    if (saveCount > 0) {
      size_t requiredSize = 0;  // value will default to 0, if not set yet in NVS
      err = nvs_get_blob(nvsHandle, STORAGE_TAG, NULL, &requiredSize);
      if (err == ESP_ERR_NVS_NOT_FOUND) {
        break;
      }
      if (err != ESP_OK) {
        APP_LOGE("[NvsFlash]", "loadData read \"%s\" size failed %d", STORAGE_TAG, err);
        break;
      }
      if (requiredSize != loadSize) {
        APP_LOGE("[NvsFlash]", "loadData read \"%s\" size got unexpected value", STORAGE_TAG);
        break;
      }
      // read previously saved data
      err = nvs_get_blob(nvsHandle, STORAGE_TAG, out, &requiredSize);
      if (err != ESP_OK) {
        // _setDefaultConfig();  // already set default in constructor
        APP_LOGE("[NvsFlash]", "loadData read \"%s\" content failed %d", STORAGE_TAG, err);
        break;
      }
      succeeded = true;
    }

  } while(false);

  // close nvs
  if (nvsOpened) nvs_close(nvsHandle);

  return succeeded;
}

bool NvsFlash::saveData(const char *STORAGE_TAG, const void *data, size_t saveSize, const char *SAVE_COUNT_TAG)
{
  bool succeeded = false;
  bool nvsOpened = false;

  nvs_handle nvsHandle;
  esp_err_t err;

  do {
    // open nvs
    err = nvs_open(APP_STORAGE_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) {
      APP_LOGE("[NvsFlash]", "saveData open nvs failed %d", err);
      break;
    }
    nvsOpened = true;

    // read save count if SAVE_COUNT_TAG given
    uint16_t saveCount = 0; // value will default to 0, if not set yet in NVS
    if (SAVE_COUNT_TAG) {
      err = nvs_get_u16(nvsHandle, SAVE_COUNT_TAG, &saveCount);
      if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        APP_LOGE("[NvsFlash]", "saveData read save-count \"%s\" failed %d", SAVE_COUNT_TAG, err);
        break;
      }
    }

    // write data
    err = nvs_set_blob(nvsHandle, STORAGE_TAG, data, saveSize);
    if (err != ESP_OK) {
      APP_LOGE("[NvsFlash]", "saveData write \"%s\" content failed %d", STORAGE_TAG, err);
      break;
    }

    // write save count
    if (SAVE_COUNT_TAG) {
      ++saveCount;
      err = nvs_set_u16(nvsHandle, SAVE_COUNT_TAG, saveCount);
      if (err != ESP_OK) {
          APP_LOGE("[NvsFlash]", "saveData write save-count \"%s\" failed %d", SAVE_COUNT_TAG, err);
          break;
      }
    }

    // commit written value.
    err = nvs_commit(nvsHandle);
    if (err != ESP_OK) {
      ESP_LOGE("[NvsFlash]", "saveData commit failed %d", err);
      break;
    }
    succeeded = true;

  } while(false);

  // close nvs
  if (nvsOpened) nvs_close(nvsHandle);

  return succeeded;
}
