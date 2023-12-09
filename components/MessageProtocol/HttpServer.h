/*
 * HttpServer: Wrap the mongoose lib
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _HTTPSERVER_H
#define _HTTPSERVER_H

#include "ProtocolMessageInterpreter.h"
#include "ProtocolDelegate.h"
#include "AppUpdateProtocol.h"
#include "esp_https_server.h"
#include "SocketSessionPool.h"

enum RequestType {
    GET,
    POST,
    WS
};

class HttpServer : public ProtocolDelegate, public AppUpdateRetCodeHandler
{
public:
    // constructor
    HttpServer(size_t maxNumOfClients=SESSION_POOL_CAPACITY);

    // config, init and deinit
    // void init();
    // void deinit();

    void start(bool enableSSL = false);
    void stop();
    size_t websocketConnectionCount();

    // --- ProtocolDelegate interface
    virtual void setup();
    virtual void replyMessage(const void *data, size_t length, void *userdata, int type);

    // --- AppUpdateRetCodeHandler interface
    virtual void handleUpdateRetCode(int code, const char *msg, int value);

public:
    // for event handler
    // void onAccept(struct mg_connection *nc);
    // void onHttpRequest(struct mg_connection *nc, struct http_message *hm);
    // void onWebsocketHandshakeRequest(struct mg_connection *nc);
    // void onWebsocketHandshakeDone(struct mg_connection *nc);
    // void onWebsocketFrame(struct mg_connection *nc, struct websocket_message *wm);
    // void onClose(struct mg_connection *nc);

    void onHttpRequest(httpd_req_t *req, RequestType type, const char *data, int size);
    void onWebsocketHandshakeDone(httpd_req_t *req, RequestType type, const char *data, int size);
    void onWebsocketFrame(httpd_req_t *req, RequestType type, const uint8_t *data, int size);
    void onWebsocketClosed(httpd_req_t *req);

protected:
    void _ws_send(httpd_req_t *req, uint8_t *data, size_t length, int type=1, bool async=false);

protected:
    bool                     _started;
    bool                     _sslEnabled;
    size_t                   _maxNumOfClients;
    httpd_handle_t           _server;
};

#endif // _HTTPSERVER_H