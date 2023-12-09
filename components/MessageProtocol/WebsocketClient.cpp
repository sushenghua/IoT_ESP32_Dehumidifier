/*
 * WebsocketClient: Wrap the mongoose lib
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "WebsocketClient.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "AppLog.h"
#include "Wifi.h"
#include "System.h"

#define TAG     "[WebsocketClient]"

/////////////////////////////////////////////////////////////////////////////////////////
// ------ using ssl connection creditial data from header file
/////////////////////////////////////////////////////////////////////////////////////////
#if USING_HEADER_FILE_SSL_CREDENTIAL
#include "certs/mqtt_crt.h"
#include "certs/mqtt_key.h"
#include "certs/ski.h"
#include "Crypto.h"

// void print_test()
// {
//     printf("===> mqtt crt (%d) : \n%s\n", strlen(mqttCrt), mqttCrt);
//     printf("===> mqtt key (%d) : \n%s\n", strlen(mqttKey), mqttKey);
// }

#define CRT_CACHE_SIZE 2048
#define VSIZE 32
char            _crtCache[CRT_CACHE_SIZE];
size_t          _crtLength = 0;

void _decryptCrt()
{
    uint32_t sih = 1027434326;
    uint32_t sil = 1027424597;

    unsigned char   v1[VSIZE];
    unsigned char   v2[VSIZE];
    unsigned char   _v1[VSIZE];
    unsigned char   _v2[VSIZE];

    size_t len = strlen(ski) / 2;
    memcpy(v1, ski,       len);
    memcpy(v2, ski + len, len);

    memcpy(v1 + len, &sih, 4); v1[len + 4] = '\0';
    memcpy(v2 + len, &sil, 4); v2[len + 4] = '\0';

    decode(v1, strlen((char*)v1), _v1, &len);
    decode(v2, strlen((char*)v2), _v2, &len);

#ifdef DEBUG_CRYPT
    ESP_LOG_BUFFER_HEXDUMP("v1", v1, len + 5, ESP_LOG_INFO);
    ESP_LOG_BUFFER_HEXDUMP("v2", v2, len + 5, ESP_LOG_INFO);
    APP_LOGC("[decryptCrt]", "_v1: %s", (char*)_v1);
    APP_LOGC("[decryptCrt]", "_v2: %s", (char*)_v2);
#endif

    decryptBase64(mqttCrt, strlen(mqttCrt), '\n', _v1, _v2, _crtCache, &_crtLength);

#ifdef DEBUG_CRYPT
    APP_LOGC("[decryptCrt]", "calculated crtCahe len: %d", _crtLength);
    // APP_LOGC("[decryptCrt]", "mqttKey len: %d, crtCahe len: %d", strlen(mqttKey), strlen(_crtCache));
    // APP_LOGC("[decryptCrt]", "decrypted crt: \n%s", _crtCache);
#endif
}
#endif // USING_HEADER_FILE_SSL_CREDENTIAL


/////////////////////////////////////////////////////////////////////////////////////////
// ------ handler for websocket client
/////////////////////////////////////////////////////////////////////////////////////////

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t _wscEventGroup;

/* The event group allows multiple bits for each event, but we only care about two event
   - we are connected to the AP with an IP
   - we failed to connect after the maximum amount of retries */
static const int WSC_INITED_BIT    = BIT0;
static const int WSC_CONNECTED_BIT = BIT1;

static void wsclient_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *event = (esp_websocket_event_data_t *)event_data;
    esp_websocket_client_handle_t client = event->client;

    WebsocketClient *wsClient = static_cast<WebsocketClient*>(event_handler_arg);

    switch (event_id) {

    case WEBSOCKET_EVENT_BEFORE_CONNECT:
        wsClient->onConnecting(event);
        break;

    case WEBSOCKET_EVENT_CONNECTED:
        wsClient->onConnected(event);
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        wsClient->onDisconnected(event);
        break;

    case WEBSOCKET_EVENT_DATA:
        wsClient->onRxData(event);
        break;

    case WEBSOCKET_EVENT_ERROR:
        wsClient->onError(event);
        break;
    }
}


/////////////////////////////////////////////////////////////////////////////////////////
// ------ WebsocketClient class
/////////////////////////////////////////////////////////////////////////////////////////
// static const char* CONFIG_WEBSOCKET_URI = "appsgenuine.com";
static const char* CONFIG_WEBSOCKET_URI = "ws://192.168.10.20:4040";
// static const char* CONFIG_WEBSOCKET_URI = "ws://echo.websocket.events";
#define WSC_SEMAPHORE_TAKE_WAIT_TICKS    1000
static SemaphoreHandle_t _wscSemaphore = 0;

WebsocketClient::WebsocketClient()
: _inited(false)
, _started(false)
, _sslEnabled(false)
, _connected(false)
, _reinitRequired(false)
, _client(NULL)
, _config({
    .uri                    = CONFIG_WEBSOCKET_URI,
    .host                   = NULL,
    .port                   = 0,
    .username               = NULL,
    .password               = NULL,
    .path                   = NULL,
    .disable_auto_reconnect = true,
    .buffer_size            = 4100,
    .keep_alive_enable      = true,
    .reconnect_timeout_ms   = 5000,
    .network_timeout_ms     = 5000,
})
{
    _wscSemaphore = xSemaphoreCreateMutex();
    // _wscEventGroup = xEventGroupCreate();
}

void WebsocketClient::init(bool enableSSL)
{
    if (!xSemaphoreTake(_wscSemaphore, WSC_SEMAPHORE_TAKE_WAIT_TICKS)) return;
    if (!_inited) {

        _wscEventGroup = xEventGroupCreate();
        // _config.uri = CONFIG_WEBSOCKET_URI;

        if (enableSSL) {
            extern const char servercert_start[] asm("_binary_servercert_pem_start");
            extern const char servercert_end[]   asm("_binary_servercert_pem_end");
            _config.cert_pem = servercert_start;
            _config.client_cert_len = servercert_end - servercert_start;

            extern const char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
            extern const char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
            _config.client_key = prvtkey_pem_start;
            _config.client_key_len = prvtkey_pem_end - prvtkey_pem_start;
        }
        else {
            _config.cert_pem = NULL;
            _config.client_cert_len = 0;
            _config.client_key = NULL;
            _config.client_key_len = 0;
        }

        _sslEnabled = enableSSL;

        _client = esp_websocket_client_init(&_config);
        if (_client == NULL) { APP_LOGE(TAG, "esp_websocket_client_init failed"); return; }

        esp_err_t ret;    
        ret = esp_websocket_register_events(_client, WEBSOCKET_EVENT_ANY, 
                                            wsclient_event_handler, static_cast<void*>(this));
        if (ret != ESP_OK) { APP_LOGE(TAG, "esp_websocket_register_events failed"); return; }

        _inited = true;
        _reinitRequired = false;
        APP_LOGC(TAG, "init succeeded");
        xEventGroupSetBits(_wscEventGroup, WSC_INITED_BIT);
    }
    // else { APP_LOGW(TAG, "instance already inited"); }
    xSemaphoreGive(_wscSemaphore);
}

void WebsocketClient::deinit()
{
    if (!xSemaphoreTake(_wscSemaphore, WSC_SEMAPHORE_TAKE_WAIT_TICKS)) return;
    if (_inited) {
        stop();
        // xEventGroupClearBits(_wscEventGroup, WSC_INITED_BIT);
        esp_err_t ret = esp_websocket_client_destroy(_client);
        if (ret == ESP_OK) {
            _inited = false;
            _client = NULL;
            APP_LOGI(TAG, "instance deinited!");
        }
        else { APP_LOGE(TAG, "esp_websocket_client_destroy failed"); }
        vEventGroupDelete(_wscEventGroup);
    }
    xSemaphoreGive(_wscSemaphore);
}

void WebsocketClient::start()
{
    xEventGroupWaitBits(_wscEventGroup, WSC_INITED_BIT, false, true, portMAX_DELAY);
    if (!xSemaphoreTake(_wscSemaphore, WSC_SEMAPHORE_TAKE_WAIT_TICKS)) return;
    if (_inited) {
        if (!_started) {
            Wifi::instance()->waitConnected(); // block wait

            esp_err_t ret = esp_websocket_client_start(_client);
            if (ret == ESP_OK) {
                APP_LOGC(TAG, "client started!");
                _started = true;
            }
            else { APP_LOGE(TAG, "esp_websocket_client_start failed"); }
        }
        // else { APP_LOGC(TAG, "client already started!"); }
    }
    else { APP_LOGE(TAG, "must be inited before start"); }
    xSemaphoreGive(_wscSemaphore);
}

void WebsocketClient::stop()
{
    // if (!xSemaphoreTake(_wscSemaphore, WSC_SEMAPHORE_TAKE_WAIT_TICKS)) return;
    if (_started) {
        esp_err_t ret = esp_websocket_client_stop(_client);
        if (ret == ESP_OK) {
            _started = false;
            _connected = false;
            APP_LOGI(TAG, "client stopped!");
        }
        else { APP_LOGE(TAG, "esp_websocket_client_stop failed"); }
    }
    // xSemaphoreGive(_wscSemaphore);
}

void WebsocketClient::close()
{
    if (!xSemaphoreTake(_wscSemaphore, WSC_SEMAPHORE_TAKE_WAIT_TICKS)) return;
    esp_err_t ret = esp_websocket_client_close(_client, 1000);
    if (ret == ESP_OK) {
        APP_LOGI(TAG, "connection closed");
        _connected = false;
    }
    else { APP_LOGE(TAG, "esp_websocket_client_close failed"); }
    xSemaphoreGive(_wscSemaphore);
}

#define WS_CLIENT_SEND_TIMEOUT   5000
// #define WS_CLIENT_SEND_TIMEOUT   portMAX_DELAY

void WebsocketClient::waitConnected()
{
  xEventGroupWaitBits(_wscEventGroup, WSC_CONNECTED_BIT, false, true, portMAX_DELAY);
}

bool WebsocketClient::_sendData(const void *data, size_t len, int type)
{
    EventBits_t uxBits = xEventGroupWaitBits(_wscEventGroup, WSC_CONNECTED_BIT, false, true,
                                             WS_CLIENT_SEND_TIMEOUT/portTICK_PERIOD_MS);
    if( ( uxBits & WSC_CONNECTED_BIT ) == 0 ) return false;
    ws_transport_opcodes_t opcodetype = WS_TRANSPORT_OPCODES_NONE;
    if (type == PROTOCOL_MSG_FORMAT_TEXT) opcodetype = WS_TRANSPORT_OPCODES_TEXT;
    else if (type == PROTOCOL_MSG_FORMAT_BINARY) opcodetype = WS_TRANSPORT_OPCODES_BINARY;
    int ret = esp_websocket_client_send_with_opcode(_client, opcodetype,
                                                    (const uint8_t *)data, len,
                                                    WS_CLIENT_SEND_TIMEOUT);
    return ret != -1;
}


// -------------------------------------------------------
// --- ProtocolDelegate interface
void WebsocketClient::setup()
{}

void WebsocketClient::replyMessage(const void *data, size_t length, void *userdata, int type)
{
    _sendData(data, length, type);
}


// -------------------------------------------------------
// --- for event handler
void WebsocketClient::onConnecting(esp_websocket_event_data_t *e)
{
    APP_LOGI(TAG, "... connecting to server: %s", _config.uri);
}

void WebsocketClient::onConnected(esp_websocket_event_data_t *e)
{
    APP_LOGI(TAG, "connection established");
    xEventGroupSetBits(_wscEventGroup, WSC_CONNECTED_BIT);
    _connected = true;

    // --- test
    // System* sys = System::instance();
    // char buf[1024];
    // sprintf(buf, "{\"cmd\":\"ver_info\",\"fmw\":\"%s\",\"uid\":\"%s\",\"bdv\":\"%s\",\"retfmt\":\"json\"}",
    //         sys->firmwareName(), sys->uid(), sys->boardVersion());
    // _sendData(buf, strlen(buf), PROTOCOL_MSG_FORMAT_TEXT);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) APP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
}

void WebsocketClient::onDisconnected(esp_websocket_event_data_t *e)
{
    xEventGroupClearBits(_wscEventGroup, WSC_CONNECTED_BIT);
    _connected = false;

    APP_LOGI(TAG, "disconnected from server %s", _config.uri);
    log_error_if_nonzero("HTTP status code",  e->error_handle.esp_ws_handshake_status_code);
    if (e->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
        log_error_if_nonzero("reported from esp-tls", e->error_handle.esp_tls_last_esp_err);
        log_error_if_nonzero("reported from tls stack", e->error_handle.esp_tls_stack_err);
        log_error_if_nonzero("captured as transport's socket errno",  e->error_handle.esp_transport_sock_errno);
        _reinitRequired = true;
        // System::instance()->setWebsocketClientOpRequest(2);
    }
}

void WebsocketClient::onRxData(esp_websocket_event_data_t *e)
{
    // APP_LOGI(TAG, "received opcode = %d", e->op_code);
    if (e->op_code == WS_TRANSPORT_OPCODES_CLOSE && e->data_len == 2) {
        APP_LOGW(TAG, "Received closed message with code=%d", 256 * e->data_ptr[0] + e->data_ptr[1]);
    } else {
        // APP_LOGW(TAG, "rx: %.*s (opcode: %d, data_len: %d, payload_len: %d, payload_offset: %d)",
        //          e->data_len, (char *)e->data_ptr, e->op_code, e->data_len, e->payload_len, e->payload_offset);
    }
    if (_msgInterpreter && (e->op_code == WS_TRANSPORT_OPCODES_TEXT || e->op_code == WS_TRANSPORT_OPCODES_BINARY)) {
        _msgInterpreter->interpretSocketMsg(e->data_ptr, e->data_len, MSG_SRC_WS_CLIENT, e->op_code, NULL);
    }
}

void WebsocketClient::onError(esp_websocket_event_data_t *e)
{
    APP_LOGE(TAG, "error occurred %d", e->error_handle.esp_ws_handshake_status_code);
    log_error_if_nonzero("HTTP status code",  e->error_handle.esp_ws_handshake_status_code);
    if (e->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
        log_error_if_nonzero("reported from esp-tls", e->error_handle.esp_tls_last_esp_err);
        log_error_if_nonzero("reported from tls stack", e->error_handle.esp_tls_stack_err);
        log_error_if_nonzero("captured as transport's socket errno",  e->error_handle.esp_transport_sock_errno);
    }
}

// -------------------------------------------------------
// --- AppUpdateFirmwareFetchClient interface
#include "SharedBuffer.h"

void WebsocketClient::initUpdater()
{
    start();
}

bool WebsocketClient::sendUpdateRequest()
{
    System* sys = System::instance();
    char *buf = SharedBuffer::updaterMsgBuffer();
    sprintf(buf, "{\"cmd\":\"ver_info\",\"fmw\":\"%s\",\"uid\":\"%s\",\"bdv\":\"%s\"}",
            sys->firmwareName(), sys->uid(), sys->boardVersion());
    return _sendData(buf, strlen(buf), PROTOCOL_MSG_FORMAT_TEXT);
}

bool WebsocketClient::sendData(const void *data, size_t len)
{
    return _sendData(data, len, PROTOCOL_MSG_FORMAT_BINARY);
}

void WebsocketClient::onUpdateEnded(bool succeeded,  bool stopDelegate)
{
    if (succeeded) {
        System::instance()->setAllMessageProtocolsStopRequest();
        System::instance()->setRestartRequest();
    }
    else if (stopDelegate) System::instance()->setWebsocketClientOpRequest(1);
}

int WebsocketClient::customizedFun(const void *data)
{
    return 0;
}
