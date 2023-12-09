/*
 * AppUpdateProtocol: protocol for app updating
 * Copyright (c) 2023 Shenghua Su
 *
 */

#ifndef _APP_UPDATE_PROTOCOL_H
#define _APP_UPDATE_PROTOCOL_H

#include <stddef.h>
#include "ProtocolDelegate.h"

class AppUpdateFirmwareFetchClient: public ProtocolDelegate
{
public:
    // virtual interface
    virtual void initUpdater() = 0;
    virtual bool sendUpdateRequest() = 0;
    virtual bool sendData(const void *data, size_t len) = 0;
    // virtual void onRxData(const void *data, size_t len) = 0;
    virtual void onUpdateEnded(bool succeeded = true, bool stopDelegate = false) = 0;
    virtual int customizedFun(const void *data) = 0;
};

class AppUpdateRetCodeHandler
{
public:
    // virtual interface
    virtual void handleUpdateRetCode(int code, const char *msg, int value) = 0;
};

#endif // _APP_UPDATE_PROTOCOL_H