/*
 * WebsocketClient: Wrap the mongoose lib
 * Copyright (c) 2023 Shenghua Su
 *
 */

#ifndef _WEBSOCKET_CLIENT_H
#define _WEBSOCKET_CLIENT_H

// #include "ProtocolDelegate.h"
#include "AppUpdateProtocol.h"
#include "esp_websocket_client.h"

class WebsocketClient : public AppUpdateFirmwareFetchClient
{
public:
    // --- constructor
    WebsocketClient();

    // --- init, deinit, start, stop
    void init(bool enableSSL = false);
    void deinit();

    void start();
    void stop();
    void close();
    void waitConnected();
    bool connected() { return _connected; }
    bool reinitRequried() { return _reinitRequired; }

    // --- ProtocolDelegate interface
    virtual void setup();
    virtual void replyMessage(const void *data, size_t length, void *userdata, int type);

    // --- AppUpdateFirmwareFetchClient interface
    virtual void initUpdater();
    virtual bool sendUpdateRequest();
    virtual bool sendData(const void *data, size_t len);
    virtual void onUpdateEnded(bool succeeded = true,  bool stopDelegate = false);
    virtual int customizedFun(const void *data);

public:
    // for event handler
    void onConnecting(esp_websocket_event_data_t *e);
    void onConnected(esp_websocket_event_data_t *e);
    void onDisconnected(esp_websocket_event_data_t *e);
    void onRxData(esp_websocket_event_data_t *e);
    void onError(esp_websocket_event_data_t *e);

protected:
    bool _sendData(const void *data, size_t len, int type);

protected:
    bool                            _inited;
    bool                            _started;
    bool                            _sslEnabled;
    bool                            _connected;
    bool                            _reinitRequired;
    esp_websocket_client           *_client;
    esp_websocket_client_config_t   _config;
};

#endif // _WEBSOCKET_CLIENT_H
