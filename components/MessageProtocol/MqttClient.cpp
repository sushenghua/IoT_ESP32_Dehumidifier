/*
 * MqttClient: Wrap the mongoose lib
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "MqttClient.h"

#include "Config.h"
#include "AppLog.h"
#include "Wifi.h"
#include "SNTP.h"
#include "System.h"


/////////////////////////////////////////////////////////////////////////////////////////
// ------ helper functions
/////////////////////////////////////////////////////////////////////////////////////////
void printTopics(const MqttSubTopic *topics, uint16_t count)
{
#ifdef LOG_MQTT_TOPIC_CHANGE
    for (uint16_t i = 0; i < count; ++i) {
        printf("[%d] %s\n", i, topics[i].topic);
    }
#endif
}

void printTopics(const char **topics, uint16_t count)
{
#ifdef LOG_MQTT_TOPIC_CHANGE
    for (uint16_t i = 0; i < count; ++i) {
        printf("[%d] %s\n", i, topics[i]);
    }
#endif
}

static uint16_t _mqttMsgId = 0;
inline static uint16_t createMsgId() { return _mqttMsgId++; }


/////////////////////////////////////////////////////////////////////////////////////////
// ------ mqtt event handler
/////////////////////////////////////////////////////////////////////////////////////////
#define     TAG     "[MqttClient]"

void reconnectCountDelay(TickType_t ticks)
{
    int seconds = ticks / 1000;
    while (seconds > 0) {
        APP_LOGI(TAG, "reconnect in %d second%s", seconds, seconds > 1 ? "s" : "");
        vTaskDelay(1000/portTICK_PERIOD_MS);
        --seconds;
    }
}

static void mqtt_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    APP_LOGC(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", event_base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    MqttClient *mqttClient = static_cast<MqttClient*>(event_handler_arg);

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_BEFORE_CONNECT:
        mqttClient->onConnect(event);
        break;
    case MQTT_EVENT_CONNECTED:
        mqttClient->onConnAct(event);
        break;
    case MQTT_EVENT_DISCONNECTED:
        mqttClient->onClose(event);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        mqttClient->onSubAck(event);
        break; 
    case MQTT_EVENT_UNSUBSCRIBED:
        mqttClient->onUnsubAct(event);
        break;
    case MQTT_EVENT_PUBLISHED:
        mqttClient->onPubAck(event);
        break;
    case MQTT_EVENT_DATA:
        mqttClient->onRxPubMessage(event);
        break;
    case MQTT_EVENT_ERROR:
        APP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            APP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            APP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            APP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            APP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            APP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        APP_LOGI(TAG, "Other event id: %d", event->event_id);
        break;
    }
}


/////////////////////////////////////////////////////////////////////////////////////////
// alive guard check task
// --- keep alive is implemented in esp lib
/////////////////////////////////////////////////////////////////////////////////////////
// #define ALIVE_GUARD_TASK_PRIORITY  3
static SemaphoreHandle_t _closeProcessSemaphore = 0;
// static TaskHandle_t _aliveGuardTaskHandle = 0;
// static void alive_guard_task(void *pvParams)
// {
//     MqttClient *client = static_cast<MqttClient*>(pvParams);
//     TickType_t delayTicks = client->aliveGuardInterval() * 1000;
//     while (true) {
//         client->aliveGuardCheck();
//         vTaskDelay(delayTicks / portTICK_PERIOD_MS);
//     }
// }


/////////////////////////////////////////////////////////////////////////////////////////
// Message pub pool process loop task
/////////////////////////////////////////////////////////////////////////////////////////
#define MSG_POOL_TASK_PRIORITY     3
static TaskHandle_t _msgTaskHandle = 0;
static void msg_pool_task(void *pvParams)
{
    MessagePubPool *pool = static_cast<MessagePubPool*>(pvParams);
    while (true) {
        pool->processLoop();
        vTaskDelay(pool->loopInterval() / portTICK_PERIOD_MS);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////
// MQTT over TSL
/////////////////////////////////////////////////////////////////////////////////////////
#if USING_HEADER_FILE_SSL_CREDENTIAL

#include "certs/mqtt_crt.h"
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
// ------ MqttClient class
/////////////////////////////////////////////////////////////////////////////////////////
// FreeRTOS semaphore
static SemaphoreHandle_t _pubSemaphore = 0;

// --- default values
#define MQTT_KEEP_ALIVE_DEFAULT_VALUE                           60     // 60 seconds
#define MQTT_ALIVE_GUARD_REGULAR_INTERVAL_DEFAULT               (MQTT_KEEP_ALIVE_DEFAULT_VALUE / 2)
#define MQTT_ALIVE_GUARD_OFFLINE_INTERVAL_DEFAULT               5
#define MQTT_RECONNECT_DEFAULT_DELAY_TICKS                      10000   // 1 second
#define MQTT_SERVER_UNAVAILABLE_RECONNECT_DEFAULT_DELAY_TICKS   300000 // 5 minutes
// static const char* MQTT_SERVER_ADDR =                           "192.168.0.99:8883";
static const char* MQTT_SERVER_ADDR =                           "appsgenuine.com:8883";

MqttClient::MqttClient()
: _inited(false)
, _connected(false)
, _subscribeImmediatelyOnConnected(true)
, _reconnectTicksOnServerUnavailable(MQTT_SERVER_UNAVAILABLE_RECONNECT_DEFAULT_DELAY_TICKS)
, _reconnectTicksOnDisconnection(MQTT_RECONNECT_DEFAULT_DELAY_TICKS)
, _aliveGuardInterval(MQTT_ALIVE_GUARD_REGULAR_INTERVAL_DEFAULT)
{
    // init config
    _config.broker.address.uri = MQTT_SERVER_ADDR;
    _config.session.keepalive = MQTT_KEEP_ALIVE_DEFAULT_VALUE;
    _config.credentials.client_id = System::instance()->uid();

    // init topic cache
    _topicsSubscribed.clear();
    _topicsToSubscribe.clear();
    _topicsToUnsubscribe.clear();

    // message publish pool
    _msgPubPool.setPubDelegate(this);

#if USING_HEADER_FILE_SSL_CREDENTIAL
    // crt
    _decryptCrt();
#endif
}

void MqttClient::setServerAddress(const char* serverAddress)
{
    _config.broker.address.uri = serverAddress;
}

void MqttClient::setOnServerUnavailableReconnectDelay(TickType_t delayTicks)
{
    _reconnectTicksOnServerUnavailable = delayTicks;
}

void MqttClient::setOnDisconnectedReconnectDelay(TickType_t delayTicks)
{
    _reconnectTicksOnDisconnection = delayTicks;
}

void MqttClient::setSubscribeImmediateOnConnected(bool immediate)
{
    _subscribeImmediatelyOnConnected = immediate;
}

void MqttClient::setClientId(const char *clientId)
{
    _config.credentials.client_id = clientId;
}

void MqttClient::setCleanSession(bool cleanSenssion)
{
    _config.session.disable_clean_session = !cleanSenssion;
}

void MqttClient::setUserPassword(const char *user, const char *password)
{
    _config.credentials.username = user;
    _config.credentials.authentication.password = password;
}

void MqttClient::setKeepAlive(uint16_t value)
{
    _config.session.keepalive = value;
}

void MqttClient::setLastWill(const char* topic, uint8_t qos, const char* msg, bool retain)
{
    _config.session.last_will.topic = topic;
    _config.session.last_will.msg = msg;
    _config.session.last_will.msg_len = strlen(msg);
    _config.session.last_will.qos = qos;
    _config.session.last_will.retain = retain;
}

void MqttClient::init()
{
    if (!_inited) {
        APP_LOGI(TAG, "init, free RAM: %ld bytes", esp_get_free_heap_size());
#if USING_HEADER_FILE_SSL_CREDENTIAL
        _config.broker.verification.certificate = _crtCache;
        _config.credentials.authentication.certificate = mqttCrt;
        _config.credentials.authentication.key = mqttKey;
#endif
        _client = esp_mqtt_client_init(&_config);
        if (_client != NULL) {
            esp_err_t ret = esp_mqtt_client_register_event(_client,
                                                           (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, 
                                                            mqtt_event_handler, static_cast<void*>(this));
            if (ret == ESP_OK) _inited = true;
            else APP_LOGE(TAG, "esp_mqtt_client_register_event failed");
        }
        else APP_LOGE(TAG, "esp_mqtt_client_init failed");
    }
}

void MqttClient::deinit()
{
    if (_inited) {
        APP_LOGI(TAG, "deinit client ...");
        esp_err_t ret;
        ret = esp_mqtt_client_stop(_client);
        if (ret != ESP_OK) APP_LOGE(TAG, "esp_mqtt_client_stop failed");
        ret = esp_mqtt_client_unregister_event(_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler);
        if (ret != ESP_OK) APP_LOGE(TAG, "esp_mqtt_client_unregister_event failed");
        ret = esp_mqtt_client_destroy(_client);
        if (ret != ESP_OK) APP_LOGE(TAG, "esp_mqtt_client_destroy failed");
        _inited = false;
    }
}

void MqttClient::start()
{
    // SNTP::waitSynced();    // block wait time sync
    _makeConnection();
    _pubSemaphore = xSemaphoreCreateMutex();
    _closeProcessSemaphore = xSemaphoreCreateMutex();
    // xTaskCreate(&alive_guard_task, "alive_guard_task", 4096, this, ALIVE_GUARD_TASK_PRIORITY, &_aliveGuardTaskHandle);
    xTaskCreate(&msg_pool_task, "msg_pool_task", 4096, &_msgPubPool, MSG_POOL_TASK_PRIORITY, &_msgTaskHandle);
}

const SubTopics & MqttClient::topicsSubscribed()
{
    return _topicsSubscribed;
}

bool MqttClient::_makeConnection()
{
    if (_inited) {
        Wifi::instance()->waitConnected(); // block wait wifi
        esp_err_t ret = esp_mqtt_client_start(_client);
        if (ret != ESP_OK) {
            APP_LOGE(TAG, "esp_mqtt_client_start failed");
            return false;
        }
        return true;
    }
    APP_LOGE(TAG, "must be inited before connection");
    return false;
}

void MqttClient::_closeProcess()
{
    _connected = false;
    //vTaskDelete(_aliveGuardTaskHandle);
    reconnectCountDelay(_reconnectTicksOnDisconnection);
    _makeConnection();
}

// -------------------------------------------------------
// --- MqttClientDelegate interface
void MqttClient::addSubTopic(const char *topic, uint8_t qos)
{
    _topicsToSubscribe.addSubTopic(topic, qos);
}

void MqttClient::subscribeTopics()
{
    if (_connected && _topicsToSubscribe.count > 0) {
        APP_LOGI(TAG, "subscribe to topics:");
        printTopics(_topicsToSubscribe.topics, _topicsToSubscribe.count);
        for (uint16_t i = 0; i < _topicsToSubscribe.count; ++i) {
            esp_mqtt_client_subscribe(_client, 
                                      _topicsToSubscribe.topics[i].topic,
                                      _topicsToSubscribe.topics[i].qos);
        }
    }
}

void MqttClient::addUnsubTopic(const char *topic)
{
    _topicsToUnsubscribe.addUnsubTopic(topic);
}

void MqttClient::unsubscribeTopics()
{
    if (_connected && _topicsToUnsubscribe.count > 0) {
        APP_LOGI(TAG, "unsubscribe to topics:");
        printTopics(_topicsToUnsubscribe.topics, _topicsToUnsubscribe.count);
        for (uint16_t i = 0; i < _topicsToSubscribe.count; ++i) {
            esp_mqtt_client_unsubscribe(_client,
                                        _topicsToUnsubscribe.topics[i]);
        }
    }
}

#define MSG_PUB_SEMAPHORE_TAKE_WAIT_TICKS  5000   /// TickType_t

void MqttClient::publish(const char *topic, const void *data, size_t len, uint8_t qos, bool retain, bool dup)
{
    (void)dup;
    if (!_connected) return;
    if (xSemaphoreTake(_pubSemaphore, MSG_PUB_SEMAPHORE_TAKE_WAIT_TICKS)) {
        // uint16_t msgId = qos > 0 ? createMsgId() : 0;
        int msgId = esp_mqtt_client_publish(_client, topic, (const char*)data, len, qos, retain);
        if (qos > 0) {
            _msgPubPool.addMessage(msgId, topic, data, len, qos, retain);
        }
        xSemaphoreGive(_pubSemaphore);
#ifdef LOG_MQTT_TX
        APP_LOGC(TAG, "pub message (msg_id: %d, qos: %d) %s: %.*s", msgId, qos, topic,
                 len, (const char*)data);
#endif
    }
}

bool MqttClient::hasUnackPub()
{
    APP_LOGC(TAG, "pool message count: %d", _msgPubPool.poolMessageCount());
    return _msgPubPool.poolMessageCount() > 0;
}

// -------------------------------------------------------
// --- MessagePubDelegate interface
void MqttClient::repubMessage(PoolMessage *message)
{
    if (!_connected) return;
    if (xSemaphoreTake(_pubSemaphore, MSG_PUB_SEMAPHORE_TAKE_WAIT_TICKS)) {
        esp_mqtt_client_publish(_client,
                                message->topic,
                                (const char *)message->data,
                                message->length,
                                message->qos,
                                message->retain);
        message->pubCount++;
        xSemaphoreGive(_pubSemaphore);
#ifdef LOG_MQTT_RETX
        APP_LOGE(TAG, "repub message (msg_id: %d) %s: %.*s", message->msgId,
                 message->topic, message->length, (const char*)message->data);
#endif
    }
}

// -------------------------------------------------------
// --- for event handler
void MqttClient::onConnect(esp_mqtt_event_t *e)
{
    APP_LOGI(TAG, "... connecting to server: %s (client id: %s)",
             _config.broker.address.uri, _config.credentials.client_id);
    // APP_LOGI(TAG, "... connecting to server: *** (client id: uid)");
}

void MqttClient::onConnAct(esp_mqtt_event_t *e)
{
    esp_mqtt_connect_return_code_t ret_code = e->error_handle->connect_return_code;
    if (ret_code == MQTT_CONNECTION_ACCEPTED) {
        APP_LOGI(TAG, "connection established");
        _connected = true;
        if (_subscribeImmediatelyOnConnected) {
            subscribeTopics();
        }
        _recentActiveTime = time(NULL);
    }
    else {
        APP_LOGE(TAG, "connection error: %d", ret_code);
        if (ret_code == MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE) {
            reconnectCountDelay(_reconnectTicksOnServerUnavailable);
            _makeConnection();
        }
    }
}

void MqttClient::onPubAck(esp_mqtt_event_t *e)
{
    // APP_LOGI(TAG, "message QoS(1) Pub acknowledged (msg_id: %d)", msg->message_id);
    _msgPubPool.drainPoolMessage(e->msg_id);
    _recentActiveTime = time(NULL);
}

void MqttClient::onSubAck(esp_mqtt_event_t *e)
{
    APP_LOGI(TAG, "subscription acknowledged");
    _topicsSubscribed.addSubTopics(_topicsToSubscribe);
    _topicsToSubscribe.clear();
    APP_LOGI(TAG, "all subscribed topics:");
    printTopics(_topicsSubscribed.topics, _topicsSubscribed.count);
    _recentActiveTime = time(NULL);
}

void MqttClient::onUnsubAct(esp_mqtt_event_t *e)
{
    APP_LOGI(TAG, "unsubscription acknowledged");
    _topicsSubscribed.removeUnsubTopics(_topicsToUnsubscribe);
    _topicsToUnsubscribe.clear();
    _recentActiveTime = time(NULL);
}

void MqttClient::onRxPubMessage(esp_mqtt_event_t *e)
{
#ifdef LOG_MQTT_RX
    printf("\n");
    APP_LOGC(TAG, "got incoming message (msg_id: %d) %.*s: %.*s", e->msg_id,
             (int) e->topic_len, e->topic, (int) e->data_len, e->data);
#endif
    if (_msgInterpreter) {
        _msgInterpreter->interpretMqttMsg(e->topic, e->topic_len, e->data, e->data_len);
    }
    _recentActiveTime = time(NULL);
}

#define CLOSE_SEMAPHORE_TAKE_WAIT_TICKS    1000

void MqttClient::onClose(esp_mqtt_event_t *e)
{
    APP_LOGI(TAG, "connection to server %s closed (client id: %s)", 
             _config.broker.address.uri, _config.credentials.client_id);
    // APP_LOGI(TAG, "connection to server *** closed");

    if (xSemaphoreTake(_closeProcessSemaphore, CLOSE_SEMAPHORE_TAKE_WAIT_TICKS)) {
        _closeProcess();
        xSemaphoreGive(_closeProcessSemaphore);
    }
}

// -------------------------------------------------------
// --- procotol actions unavailable at application level
void MqttClient::onPubRec(esp_mqtt_event_t *e)
{
    // APP_LOGI(TAG, "message QoS(2) Pub-Receive acknowledged (msg_id: %d)", msg->message_id);
    // mg_mqtt_pubrel(nc, event->msg_id);
    _recentActiveTime = time(NULL);
}

void MqttClient::onPubComp(esp_mqtt_event_t *e)
{
    // APP_LOGI(TAG, "message QoS(2) Pub-Complete acknowledged (msg_id: %d)", msg->message_id);
    onPubAck(e);
    _recentActiveTime = time(NULL);
}

void MqttClient::onPingResp(esp_mqtt_event_t *e)
{
#ifdef LOG_MQTT_PING_PONG
    APP_LOGI(TAG, "got ping response");
#endif
    _recentActiveTime = time(NULL);
}

void MqttClient::onTimeout(esp_mqtt_event_t *e)
{
    APP_LOGI(TAG, "connection timeout");
}

// -------------------------------------------------------
// --- AppUpdaterProtocol interface
#include "SharedBuffer.h"

#define APP_UPDATE_TOPIC             "api/update"
size_t _rxTopicLen;
static char _updateDrxDataTopic[48];   // device rx
static char _updateDtxDataTopic[48];   // device tx
static char _updateCrxCodeTopic[48];   // client (mobile app) rx

void MqttClient::initUpdater()
{
  System* sys = System::instance();
  sprintf(_updateDrxDataTopic, "%s/%s/drx/%s", APP_UPDATE_TOPIC, sys->uid(), sys->boardVersion());
  sprintf(_updateDtxDataTopic, "%s/%s/dtx/%s", APP_UPDATE_TOPIC, sys->uid(), sys->boardVersion());
  _rxTopicLen = strlen(_updateDrxDataTopic);

  sprintf(_updateCrxCodeTopic, "api/updatecode/%s", sys->uid());
}

bool MqttClient::sendUpdateRequest()
{
    addUnsubTopic(MqttClientDelegate::cmdTopic());
    addUnsubTopic(MqttClientDelegate::strCmdTopic());
    unsubscribeTopics();

    addSubTopic(_updateDrxDataTopic);
    subscribeTopics();
    System* sys = System::instance();
    char *buf = SharedBuffer::updaterMsgBuffer();
    sprintf(buf, "{\"cmd\":\"ver_info\",\"fmw\":\"%s\",\"uid\":\"%s\",\"bdv\":\"%s\"}",
            sys->firmwareName(), sys->uid(), sys->boardVersion());
    publish(APP_UPDATE_TOPIC, buf, strlen(buf), 1);
    return true;
}

bool MqttClient::sendData(const void *data, size_t len)
{
    publish(_updateDtxDataTopic, data, len, 1);
    return true;
}

void MqttClient::onUpdateEnded(bool succeeded,  bool stopDelegate)
{
    // unsubscribe update topic
    addUnsubTopic(_updateDrxDataTopic);
    unsubscribeTopics();

    // if unsucceeded, subscirbe to cmd topic
    if (!succeeded) {
        addSubTopic(MqttClientDelegate::cmdTopic());
        addSubTopic(MqttClientDelegate::strCmdTopic());
        subscribeTopics();
    }
}

int MqttClient::customizedFun(const void *data)
{
    const char *topic = (const char *)data;
    return strncmp(topic, _updateDrxDataTopic, _rxTopicLen) == 0;
}

// -------------------------------------------------------
// --- AppUpdateRetCodeHandler interface
void MqttClient::handleUpdateRetCode(int code, const char *msg, int value)
{
    char *buf = SharedBuffer::updaterMsgBuffer();
    sprintf(buf, "{\"code\":\"%d\",\"msg\":\"%s\",\"val\":\"%d\"}", code, msg, value);
    publish(_updateCrxCodeTopic, buf, strlen(buf), 1);
}

// -----------------------------------------
// --- keep alive is implemented in esp lib
// -----------------------------------------
// void MqttClient::aliveGuardCheck()
// {
//     // APP_LOGI(TAG, "alive guard check");
//     if (_connected) {
//         time_t timeNow = time(NULL);
//         if (timeNow - _recentActiveTime > _handShakeOpt.keep_alive) {
//             // server does not response, connection dead
//             APP_LOGI(TAG, "client has received no response for a while, connection considered to be lost");
//             if (xSemaphoreTake(_closeProcessSemaphore, CLOSE_SEMAPHORE_TAKE_WAIT_TICKS)) {
//                 if (_connected) {
//                     // mg_mqtt_disconnect(_manager.active_connections);
//                 // mg_set_timer(_manager.active_connections, mg_time() + 1);
//                     // _manager.active_connections->flags |= MG_F_SEND_AND_CLOSE;
//                 }
//                 xSemaphoreGive(_closeProcessSemaphore);
//             }
//         }
//         else if (_connected && timeNow - _recentActiveTime > _aliveGuardInterval) {
// #ifdef LOG_MQTT_PING_PONG
//             APP_LOGI(TAG, "no activity recently, ping to server");
// #endif
//             // mg_mqtt_ping(_manager.active_connections);
//         }
//     }
// }
