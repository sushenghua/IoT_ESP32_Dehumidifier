/*
 * AppUpdater app firmware updater
 * Copyright (c) 2016 Shenghua Su
 *
 */

#ifndef _APP_UPDATER_H
#define _APP_UPDATER_H

#include "MqttClientDelegate.h"
#include "AppUpdateProtocol.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"


class AppUpdater
{
public:
    // type
    enum UpdateState {
        UPDATE_STATE_IDLE,
        UPDATE_STATE_WAIT_VERSION_INFO,
        UPDATE_STATE_WAIT_DATA,
        UPDATE_STATE_WAIT_VERIFY_BITS
    };
    struct WriteFlag {
        size_t index;
        size_t amount;
    };
    typedef uint32_t VersionNoType;

public:
    AppUpdater();
    void init();
    int customizedFun(const void *data);
    void setUpdateDelegate(AppUpdateFirmwareFetchClient *delegate,
                           AppUpdateRetCodeHandler      *codeHandler)
    { _delegate = delegate; _codeHandler = codeHandler; }
    void update();
    void updateLoop(const char* data, size_t dataLen);
    bool isUpdating() { return _state != UPDATE_STATE_IDLE; }

protected:
    void _retCode(int code, const char *msg, int value = 0);
    bool _sendUpdateCmd();
    bool _beforeUpdateCheck();
    bool _prepareUpdate();
    void _onUpdateEnded(bool succeeded, bool stopDelegate=false);
    void _onRxDataComplete();
    bool _verifyData(const char *verifyBits, size_t length);

protected:
    UpdateState                     _state;
    const VersionNoType             _currentVersion;
    size_t                          _newVersionSize;
    WriteFlag                       _writeFlag;
    esp_ota_handle_t                _updateHandle;
    const esp_partition_t          *_updatePartition;
    AppUpdateFirmwareFetchClient   *_delegate;
    AppUpdateRetCodeHandler        *_codeHandler;
};

#endif // _APP_UPDATER_H
