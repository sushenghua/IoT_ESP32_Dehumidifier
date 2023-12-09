/*
 * HttpServer: Wrap the mongoose lib
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "HttpServer.h"
#include "esp_system.h"
#include "AppLog.h"
#include "Wifi.h"
// #include "SocketSessionPool.h"

#define TAG     "[HttpServer]"
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/////////////////////////////////////////////////////////////////////////////////////////
// ------ handler for HTTP Server
/////////////////////////////////////////////////////////////////////////////////////////

// -------------------------------------------------------
// ------ GET request handler
static esp_err_t _http_get_handler(httpd_req_t *req)
{
    HttpServer *httpServer = static_cast<HttpServer*>(req->user_ctx);
    httpServer->onHttpRequest(req, GET, NULL, 0);
    return ESP_OK;
}

// ------ GET request handler structure
static httpd_uri_t _http_uri_get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = _http_get_handler,
    .user_ctx = NULL
};

// -------------------------------------------------------
// ------ POST request handler

// ------ buffer for data transmission
#define HTTP_REQUEST_BUF_SIZE   1024
static char _httpReqBuf[HTTP_REQUEST_BUF_SIZE];

static esp_err_t _http_post_handler(httpd_req_t *req)
{
    // truncate if content length larger than the buffer
    size_t recv_size = MIN(req->content_len, sizeof(_httpReqBuf));

    // httpd_req_recv() accepts char* only, but content could be any binary
    // data (needs type casting). In case of string data, null termination
    // will be absent, and content length would give length of string
    int ret = httpd_req_recv(req, _httpReqBuf, recv_size);

    if (ret < 0) {
        if (ret == HTTPD_SOCK_ERR_INVALID) {
            // on socket err, httpd_req_recv() can be called again, or simply
            // respond with an HTTP 408 (Request Timeout) error
            httpd_resp_send_408(req);
        }
        // returning ESP_FAIL will ensure that the underlying socket is closed
        return ESP_FAIL;
    }

    HttpServer *httpServer = static_cast<HttpServer*>(req->user_ctx);
    httpServer->onHttpRequest(req, POST, _httpReqBuf, ret);
    return ESP_OK;
}

// ------ POST request handler structure
static httpd_uri_t _http_uri_post = {
    .uri      = "/",
    .method   = HTTP_POST,
    .handler  = _http_post_handler,
    .user_ctx = NULL
};

// -------------------------------------------------------
// ------ websocket request handler
// #define SOCKET_RX_SEMAPHORE_TAKE_WAIT_TICKS    1000
// #define SOCKET_TX_SEMAPHORE_TAKE_WAIT_TICKS    2000
// static SemaphoreHandle_t _socketSemaphore    = 0;

SocketSessionPool _socketSessionPool;

static esp_err_t _http_ws_handler(httpd_req_t *req)
{
    HttpServer *httpServer = static_cast<HttpServer*>(req->user_ctx);

    // --- handshake event
    if (req->method == HTTP_GET) {
        httpServer->onWebsocketHandshakeDone(req, GET, NULL, 0);
        int socketFd = httpd_req_to_sockfd(req);
        if (socketFd != -1) _socketSessionPool.addSession(socketFd);
        return ESP_OK;
    }

    // --- parse websocket frame
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // if ( ! xSemaphoreTake(_socketSemaphore, SOCKET_RX_SEMAPHORE_TAKE_WAIT_TICKS) ) return ESP_ERR_TIMEOUT;
    // --- read frame size
    //     set param 'max_len = 0' to get the frame len
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    // ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);

    // --- read frame content
    uint8_t *buf = NULL;
    if (ws_pkt.len > 0) {
        // ws_pkt.len + 1 is for NULL termination as we are expecting a string
        buf = (uint8_t *)(calloc(1, ws_pkt.len + 1));
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
    
        // set param 'max_len = ws_pkt.len' to get the frame payload
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
    }

    // --- direct to HttpServer object accordingly
    // APP_LOGC(TAG, "Packet type: %d", ws_pkt.type);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        httpServer->onWebsocketFrame(req, WS, ws_pkt.payload, ws_pkt.len);
    }
    else if (ws_pkt.type == HTTPD_WS_TYPE_PING) {}
    else if (ws_pkt.type == HTTPD_WS_TYPE_PONG) {}
    else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        httpServer->onWebsocketClosed(req);
        int socketFd = httpd_req_to_sockfd(req);
        if (socketFd != -1) _socketSessionPool.drainSession(socketFd);
    }
    
    // --- release resource
    free(buf);

    // xSemaphoreGive(_socketSemaphore);

    return ESP_OK;
}

// ------ websocket request handler structure
static httpd_uri_t _http_uri_ws = {
    .uri                        = "/ws",
    .method                     = HTTP_GET,
    .handler                    = _http_ws_handler,
    .user_ctx                   = NULL,
    .is_websocket               = true,
    .handle_ws_control_frames   = true
};

// -------------------------------------------------------
// ------ async job structure
// structure holding server handle and internal socket fd in order
// to use out of request send
struct ws_job_arg {
    httpd_handle_t      hd;
    int                 fd;
    httpd_ws_frame_t    pkt;
    // void clearPkt() { memset(&pkt, 0, sizeof(httpd_ws_frame_t)); }
};

// ------ async send function, which we put into the httpd work queue
static void async_ws_send_job(void *arg)
{
    ws_job_arg *job_arg = (ws_job_arg*)arg;

    esp_err_t ret = httpd_ws_send_frame_async(job_arg->hd, job_arg->fd, &job_arg->pkt);
    if (ret != ESP_OK) {
        APP_LOGE(TAG, "httpd_ws_send_frame_async failed with %d", ret);
    }
    free(job_arg);
}

static esp_err_t create_async_ws_send_job(httpd_handle_t serverHandle, int fd,
                                          httpd_ws_frame_t *pkt)
{
    // --- allocate mem for job arg
    struct ws_job_arg *job_arg = (ws_job_arg *)malloc(sizeof(struct ws_job_arg));
    if (job_arg == NULL) return ESP_ERR_NO_MEM;

    // --- assign arg
    job_arg->hd = serverHandle;
    job_arg->fd = fd;
    memcpy(&job_arg->pkt, pkt, sizeof(httpd_ws_frame_t));

    // --- queue the job
    esp_err_t ret = httpd_queue_work(serverHandle, async_ws_send_job, job_arg);
    if (ret != ESP_OK) {
        APP_LOGE(TAG, "httpd_queue_work failed with %d", ret);
        free(job_arg);
    }
    return ret;
}

static esp_err_t create_async_ws_send_job(httpd_handle_t serverHandle, httpd_req_t *req,
                                          httpd_ws_frame_t *pkt)
{
    return create_async_ws_send_job(serverHandle, httpd_req_to_sockfd(req), pkt);
}


/////////////////////////////////////////////////////////////////////////////////////////
// ------ HttpServer class
/////////////////////////////////////////////////////////////////////////////////////////
HttpServer::HttpServer(size_t maxNumOfClients)
: _started(false)
, _sslEnabled(false)
, _maxNumOfClients(maxNumOfClients > SESSION_POOL_CAPACITY ? SESSION_POOL_CAPACITY : maxNumOfClients)
, _server(NULL)
{}

void HttpServer::start(bool enableSSL)
{
    if (!_started) {
        Wifi::instance()->waitConnected(); // block wait
        esp_err_t ret;

        // _socketSemaphore = xSemaphoreCreateMutex();

        if (enableSSL) {
            httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();

            config.httpd.max_open_sockets = _maxNumOfClients;

            extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
            extern const unsigned char servercert_end[]   asm("_binary_servercert_pem_end");
            config.servercert = servercert_start;
            config.servercert_len = servercert_end - servercert_start;

            extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
            extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
            config.prvtkey_pem = prvtkey_pem_start;
            config.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

            APP_LOGI(TAG, "starting SSL server on port: '%d', free RAM: %ld bytes",
                           config.httpd.server_port, esp_get_free_heap_size());
            ret = httpd_ssl_start(&_server, &config);
        }
        else {
            httpd_config_t config = HTTPD_DEFAULT_CONFIG();
            config.lru_purge_enable = true; // purge "Least Recently Used" connection
            config.max_open_sockets = _maxNumOfClients;

            APP_LOGI(TAG, "starting plain server on port: '%d', free RAM: %ld bytes",
                           config.server_port, esp_get_free_heap_size());
            ret = httpd_start(&_server, &config);
        }
        if (ret == ESP_OK) {
            APP_LOGI(TAG, "registering request handlers");
            _http_uri_get.user_ctx  = static_cast<void*>(this);
            _http_uri_post.user_ctx = _http_uri_get.user_ctx;
            _http_uri_ws.user_ctx   = _http_uri_get.user_ctx;
            httpd_register_uri_handler(_server, &_http_uri_get);
            httpd_register_uri_handler(_server, &_http_uri_post);
            httpd_register_uri_handler(_server, &_http_uri_ws);
            _socketSessionPool.clean();
            _started = true;
            _sslEnabled = enableSSL;
        }
        else {
            APP_LOGE(TAG, "error in starting server!");
        }
    }
    else {
        APP_LOGC("[HttpServer]", "server already started!");
    }
}

void HttpServer::stop()
{
    if (_started) {
        // --- stop http server
        esp_err_t ret = _sslEnabled ? httpd_ssl_stop(_server) : httpd_stop(_server);
        if (ret == ESP_OK) {
            _socketSessionPool.clean();
            _started = false;
            _server = NULL;
            APP_LOGI(TAG, "http(s) server stopped");
        }
        else {
            APP_LOGE(TAG, "failed to stop http(s) server!");
        }
    }
}

size_t HttpServer::websocketConnectionCount()
{
    return _socketSessionPool.sessionCount();
}

void HttpServer::_ws_send(httpd_req_t *req, uint8_t *data, size_t length, int type, bool async)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = length;
    ws_pkt.type = type == PROTOCOL_MSG_FORMAT_TEXT? HTTPD_WS_TYPE_TEXT : HTTPD_WS_TYPE_BINARY;

    esp_err_t ret;
    if (async) {
        ret = create_async_ws_send_job(req->handle, req, &ws_pkt);
        if (ret != ESP_OK) {
            APP_LOGE(TAG, "create_async_ws_send_job failed with %d", ret);
        } 
    }
    else {
        ret = httpd_ws_send_frame(req, &ws_pkt);
        if (ret != ESP_OK) {
            APP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
        }
    }
}

// -------------------------------------------------------
// --- request handlers
void HttpServer::onHttpRequest(httpd_req_t *req, RequestType type, const char *data, int size)
{
    if (type == GET) {
        extern const char html_start[] asm("_binary_index_html_start");
        extern const char html_end[]   asm("_binary_index_html_end");
        size_t html_len = html_end - html_start;
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, html_start, html_len);
    }
    else if (type == POST) {
        APP_LOGC(TAG, "post request with data: %s", size > 0 ? data : "null");
        const char resp[] = "POST Response ......";
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    }

//     static const char *reply_fmt =
//         "HTTP/1.0 200 OK\r\n"
//         "Connection: close\r\n"
//         "Content-Type: text/plain\r\n"
//         "\r\n"
//         "Sensor %s\n";
//     mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
//                         MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
// #ifdef LOG_HTTP
//     APP_LOGI("[HttpServer]", "http request from %s: %.*s %.*s", addr, (int) hm->method.len,
//                               hm->method.p, (int) hm->uri.len, hm->uri.p);
// #endif
//     mg_printf(nc, reply_fmt, addr);
//     nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HttpServer::onWebsocketHandshakeDone(httpd_req_t *req, RequestType type, const char *data, int size)
{
    APP_LOGI(TAG, "Websocket handshake done, new connection opened");
}

void HttpServer::onWebsocketFrame(httpd_req_t *req, RequestType type, const uint8_t *data, int size)
{
#ifdef LOG_WEBSOCKET_MSG
    APP_LOGI(TAG, "got message: %.*s", size, data);
#endif
    if (_msgInterpreter) {
        _msgInterpreter->interpretSocketMsg(data, size, MSG_SRC_HTTP_SERVER, PROTOCOL_MSG_FORMAT_TEXT, req);
    }

    // // --- test
    // esp_err_t ret;
    // APP_LOGI(TAG, "received frame with message: %.*s", size, data);

    // if (strcmp((const char*)data, "Trigger async") == 0) {
    //     const char* txt = "async hello from server";
    //     _ws_send(req, (uint8_t*)txt, strlen(txt), 1, true);
    // }
    // else {
    //     const char* txt = "hello from server";
    //     _ws_send(req, (uint8_t*)txt, strlen(txt), 1, false);
    // }
    // // ---
}

void HttpServer::onWebsocketClosed(httpd_req_t *req)
{
    APP_LOGI("[HttpServer]", "websocket connection closed");
}

// -------------------------------------------------------
// --- ProtocolDelegate interface interface
void HttpServer::setup()
{}

void HttpServer::replyMessage(const void *data, size_t length, void *userdata, int type)
{
    _ws_send((httpd_req_t *)userdata, (uint8_t*)data, length, type, true);
}

// -------------------------------------------------------
// --- AppUpdateRetCodeHandler interface
static int  _lastDPVal = 0;
static char _msgBuf[1024];
void HttpServer::handleUpdateRetCode(int code, const char *msg, int value)
{
    // avoid frequently send progress value back
    if (code == 12) {
        if (_lastDPVal != value) _lastDPVal = value;
        else return;
    }

    // --- this function used to send msg to remote monitor side, e.g. mobile App
    sprintf(_msgBuf, "{\"cmd\":\"UpdateRet\",\"code\":%d,\"msg\":\"%s\",\"val\":\"%d\"}", code, msg, value);
    // _ws_send(_server, (uint8_t*)data, strlen(_msgBuf), PROTOCOL_MSG_FORMAT_TEXT, true);
    // if ( ! xSemaphoreTake(_socketSemaphore, SOCKET_TX_SEMAPHORE_TAKE_WAIT_TICKS) ) return;
    _socketSessionPool.broadcastMessage(_server, _msgBuf, strlen(_msgBuf));
    // xSemaphoreGive(_socketSemaphore);
}
