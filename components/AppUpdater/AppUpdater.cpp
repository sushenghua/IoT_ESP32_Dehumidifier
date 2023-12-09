/*
 * AppUpdater app firmware updater
 * Copyright (c) 2016 Shenghua Su
 *
 */

#include "AppUpdater.h"

#include <string.h>
#include "md5.h"
#include "Wifi.h"
#include "System.h"
#include "Config.h"
#include "AppLog.h"
#include "AppUpdaterConfig.h"
#include "SharedBuffer.h"

/////////////////////////////////////////////////////////////////////////////////////////
// debug purpose
/////////////////////////////////////////////////////////////////////////////////////////
#define TAG      "[AppUpdater]"

// #define DEBUG_APP_UPDATER
#ifdef  DEBUG_APP_UPDATER
  esp_err_t debug_esp_ota_begin(const esp_partition_t* partition, size_t image_size, esp_ota_handle_t* out_handle)
  {
    return ESP_OK;
  }
  
  esp_err_t debug_esp_ota_write(esp_ota_handle_t handle, const void* data, size_t size)
  {
    return ESP_OK;
  }
  
  esp_err_t debug_esp_ota_end(esp_ota_handle_t handle)
  {
    return ESP_OK;
  }

  esp_err_t debug_esp_ota_set_boot_partition(const esp_partition_t* partition)
  {
    return ESP_OK;
  }

  #define ESP_OTA_BEGIN                  debug_esp_ota_begin
  #define ESP_OTA_END                    debug_esp_ota_end
  #define ESP_OTA_WRITE                  debug_esp_ota_write
  #define ESP_OTA_SET_BOOT_PATITION      debug_esp_ota_set_boot_partition
#else
  #define ESP_OTA_BEGIN                  esp_ota_begin
  #define ESP_OTA_END                    esp_ota_end
  #define ESP_OTA_WRITE                  esp_ota_write
  #define ESP_OTA_SET_BOOT_PATITION      esp_ota_set_boot_partition
#endif

/////////////////////////////////////////////////////////////////////////////////////////
// communicated with updater service provider and client app
/////////////////////////////////////////////////////////////////////////////////////////
#define REQUIRE_VERIFY_BIT_CMD       SIZE_MAX
#define UPDATE_RX_DATA_BLOCK_SIZE    4096   // should not exceed buf size of protocol layer

enum UpdateRetCode {
  UPDATE_OK                              = 0,
  ALREADY_LATEST,
  OTA_RUNNING_PARTITION_NOT_BOOT,
  OTA_GET_UPDATE_PARTITION_FAILED,
  OTA_SET_BOOT_PATITION_FAILED,
  OTA_BEGIN_FAILED,
  OTA_END_FAILED,
  OTA_WRITE_FAILED,
  RXDATA_MISMATCHED_WITH_REQUIRED,
  RXDATA_SIZE_LARGER_THAN_EXPECTED,
  MD5_CHECK_OK,
  MD5_CHECK_FAILED,
  DOWNLOAD_PROGRESS
};

#define      MD5_LENGTH 16
md5_context _md5Contex;
char        _md5Result[MD5_LENGTH];

// #define UPDATE_SEMAPHORE_TAKE_WAIT_TICKS    10
// static SemaphoreHandle_t _updateSemaphore = 0;

/////////////////////////////////////////////////////////////////////////////////////////
// AppUpdater implementation
/////////////////////////////////////////////////////////////////////////////////////////
AppUpdater::AppUpdater()
: _state(UPDATE_STATE_IDLE)
, _currentVersion(FIRMWARE_VERSION)
, _newVersionSize(0)
, _updateHandle(0)
, _delegate(NULL)
, _codeHandler(NULL)
{
  _writeFlag.index = 0;
  _writeFlag.amount = 0;
  // _updateSemaphore = xSemaphoreCreateMutex();
}

void AppUpdater::init()
{
  _delegate->initUpdater();
  _state = UPDATE_STATE_IDLE;
}

int AppUpdater::customizedFun(const void *data)
{
  return _delegate->customizedFun(data);
}

void AppUpdater::_onUpdateEnded(bool succeeded, bool stopDelegate)
{
  if (_delegate) _delegate->onUpdateEnded(succeeded, stopDelegate);
}

void AppUpdater::_retCode(int code, const char *msg, int value)
{
#ifdef LOG_APPUPDATER
  if (code == UPDATE_OK) APP_LOGC(TAG, "%s", msg);
  else if (code == DOWNLOAD_PROGRESS) APP_LOGI(TAG, "write data complete %d%%", value);
  else if (code == MD5_CHECK_OK) APP_LOGI(TAG, "%s", msg);
  else APP_LOGE(TAG, "%s, 0x%x", msg, value);
#else
  if (code == DOWNLOAD_PROGRESS) APP_LOGI(TAG, "write data complete %d%%", value);
  else if (code == ALREADY_LATEST) APP_LOGC(TAG, "%s", msg);
  else if (code == MD5_CHECK_OK) APP_LOGI(TAG, "%s", msg);
  else if (code != UPDATE_OK && code != DOWNLOAD_PROGRESS) APP_LOGE(TAG, "%s, 0x%x", msg, value);
#endif

  if (_codeHandler) _codeHandler->handleUpdateRetCode(code, msg, value);

  if (code != UPDATE_OK && code != DOWNLOAD_PROGRESS && code != MD5_CHECK_OK) {
    System::instance()->resumePeripherals();
  }
}

bool AppUpdater::_sendUpdateCmd()
{
  if (_delegate) return _delegate->sendUpdateRequest();
  else return false;
}

bool AppUpdater::_beforeUpdateCheck()
{
  APP_LOGI(TAG, "checking partition ...");

  const esp_partition_t *configured = esp_ota_get_boot_partition();
  const esp_partition_t *running = esp_ota_get_running_partition();

  if (configured != running) {
    _retCode(OTA_RUNNING_PARTITION_NOT_BOOT,
             "running partition is different from boot partition, cannot execute update operation");
    return false;
  }
  else {
    APP_LOGI(TAG, "running partition type %d subtype %d (offset 0x%08lx)",
                   configured->type, configured->subtype, configured->address);
    return true;
  }
}

void AppUpdater::_onRxDataComplete()
{
  bool succeeded = true;
  esp_err_t ret = ESP_OTA_END(_updateHandle);
  if (ret == ESP_OK) APP_LOGI(TAG, "ota end succeeded");
  else {
    _onUpdateEnded(false, true);
    _retCode(OTA_END_FAILED, "ota end failed!");
    succeeded = false;    // task_fatal_error();
  }

  ret = ESP_OTA_SET_BOOT_PATITION(_updatePartition);
  if (ret == ESP_OK) {
    APP_LOGI(TAG, "ota set boot partition succeeded");
  }
  else {
    _onUpdateEnded(false, true);
    _retCode(OTA_SET_BOOT_PATITION_FAILED, "ota set boot partition failed", ret);
    succeeded = false;     // task_fatal_error();
  }

  _state = UPDATE_STATE_IDLE;

  if (succeeded) {
    _retCode(UPDATE_OK, "update completed, restart ...");
    _onUpdateEnded(true, true);
    // System::instance()->setRestartRequest();
  }
}

bool AppUpdater::_prepareUpdate()
{
  _updatePartition = esp_ota_get_next_update_partition(NULL);
  APP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx",
                _updatePartition->subtype, _updatePartition->address);
  if (_updatePartition == NULL) {
    _onUpdateEnded(false, true);
    _retCode(OTA_GET_UPDATE_PARTITION_FAILED, "cannot get the update partition");
    _state = UPDATE_STATE_IDLE;
    return false;
  }

  System::instance()->pausePeripherals("updating ...");

  esp_err_t err = ESP_OTA_BEGIN(_updatePartition, OTA_SIZE_UNKNOWN, &_updateHandle);
  if (err == ESP_OK) {
    _writeFlag.index = 0;
    _writeFlag.amount = UPDATE_RX_DATA_BLOCK_SIZE;
    APP_LOGI(TAG, "ota_begin succeeded");
    return true;
  } else {
    _onUpdateEnded(false, true);
    _retCode(OTA_BEGIN_FAILED, "ota_begin failed", err);
    _state = UPDATE_STATE_IDLE;
    return false;
  }
}

bool AppUpdater::_verifyData(const char *verifyBits, size_t length)
{
  // APP_LOGC(TAG, "md5 result:   %.*s", length, _md5Result);
  // APP_LOGC(TAG, "verify bytes: %.*s", length, verifyBits);
  if (memcmp(verifyBits, _md5Result, MD5_LENGTH) == 0) {
    _retCode(MD5_CHECK_OK, "downloaded data verified OK");
    return true;
  }
  else {
    _onUpdateEnded(false, true);
    _retCode(MD5_CHECK_FAILED, "downloaded data verified fail");
    return false;
  }
}

void AppUpdater::updateLoop(const char* data, size_t dataLen)
{
  // if ( ! xSemaphoreTake(_updateSemaphore, UPDATE_SEMAPHORE_TAKE_WAIT_TICKS) ) return;

  // APP_LOGI(TAG, "update loop");
  switch (_state) {
    case UPDATE_STATE_WAIT_VERSION_INFO: {
      // --- get newVersion value
      VersionNoType newVersion = *((VersionNoType *)data);
#ifdef LOG_APPUPDATER
        APP_LOGC(TAG, "version: %ld, size: %d, cur version: %ld",
                       newVersion, *((size_t *)(data + sizeof(VersionNoType))), _currentVersion);
#endif

      // --- check if update needed
      if (newVersion > _currentVersion) {
        _newVersionSize = *((size_t *)(data + sizeof(VersionNoType)));
        if (!_prepareUpdate()) break;
        md5_starts(&_md5Contex);
        APP_LOGI(TAG, "begin downloading data ...");
        _delegate->sendData(&_writeFlag, sizeof(_writeFlag));
        _state = UPDATE_STATE_WAIT_DATA;
      }
      else {
        _state = UPDATE_STATE_IDLE;
        _retCode(ALREADY_LATEST, "already the latest version");
        _onUpdateEnded(false, true);
        System::instance()->resumePeripherals();
      }
      break;
    }

    case UPDATE_STATE_WAIT_DATA: {
      // --- data block index, size
      size_t dataIndex = *((size_t*)data);
#ifdef LOG_APPUPDATER
      APP_LOGC(TAG, "rx index: %d, rx size: %d, request index: %d, request size: %d", 
                     dataIndex, dataLen - sizeof(size_t), _writeFlag.index, _writeFlag.amount);
#endif

      // --- dataIndex check
      if (dataIndex != _writeFlag.index) {
        // _onUpdateEnded(false);
        _retCode(RXDATA_MISMATCHED_WITH_REQUIRED, "received data index mismatched with requested");
        // _state = UPDATE_STATE_IDLE;
        break;
      }

      // --- blockSize check
      const void *dataBlock = data + sizeof(size_t);
      size_t blockSize = dataLen - sizeof(size_t);
      if (_writeFlag.index + blockSize > _newVersionSize) {
        _onUpdateEnded(false, true);
        _retCode(RXDATA_SIZE_LARGER_THAN_EXPECTED, "received data size mismatched with the new version size");
        _state = UPDATE_STATE_IDLE;
        break;
      }

      // ---md5 accumulate calculation
      md5_update(&_md5Contex, (const unsigned char*)dataBlock, blockSize);

      // --- try to write ota
      esp_err_t err = ESP_OTA_WRITE(_updateHandle, dataBlock, blockSize);
      if (err == ESP_OK) {
        _writeFlag.index += blockSize;
        _retCode(DOWNLOAD_PROGRESS, "", int((_writeFlag.index/(float)_newVersionSize) * 100));
        if (_writeFlag.index == _newVersionSize) {
          // APP_LOGI(TAG, "total write binary data length : %d", _newVersionSize);
          md5_finish(&_md5Contex, (unsigned char*)_md5Result);
          _writeFlag.index = REQUIRE_VERIFY_BIT_CMD;
          _writeFlag.amount = REQUIRE_VERIFY_BIT_CMD;
          APP_LOGI(TAG, "downloading data completed");
          _delegate->sendData(&_writeFlag, sizeof(_writeFlag));
          _state = UPDATE_STATE_WAIT_VERIFY_BITS;
        }
        else {
          if (_newVersionSize - _writeFlag.index < UPDATE_RX_DATA_BLOCK_SIZE)
            _writeFlag.amount = _newVersionSize - _writeFlag.index;
          _delegate->sendData(&_writeFlag, sizeof(_writeFlag));
        }
      } else {
        _onUpdateEnded(false, true);
        _retCode(OTA_WRITE_FAILED, "ota_write failed", err);
        if (ESP_OTA_END(_updateHandle) != ESP_OK) {
          _retCode(OTA_END_FAILED, "ota end failed");
          // should exit and restart ?
        }
        _state = UPDATE_STATE_IDLE;
      }
      break;
    }

    case UPDATE_STATE_WAIT_VERIFY_BITS:
      if (_verifyData(data, dataLen)) {
        _onRxDataComplete();
      }
      break;

    case UPDATE_STATE_IDLE:
      break;

    default:
      break;
  }

  // xSemaphoreGive(_updateSemaphore);
}

void AppUpdater::update()
{
  // if ( ! xSemaphoreTake(_updateSemaphore, UPDATE_SEMAPHORE_TAKE_WAIT_TICKS) ) return;
  APP_LOGI(TAG, "receive update command");
  if (!_beforeUpdateCheck()) return;

  Wifi::instance()->waitConnected();

  _state = UPDATE_STATE_WAIT_VERSION_INFO;

  if (!_sendUpdateCmd()) _state = UPDATE_STATE_IDLE;
  // xSemaphoreGive(_updateSemaphore);
}
