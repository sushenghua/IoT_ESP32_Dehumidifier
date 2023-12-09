/*
 * MqttClient: Wrap the mongoose lib
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _MQTT_CLIENT_H
#define _MQTT_CLIENT_H

#include "MqttClientDelegate.h"
#include "MessagePubPool.h"
#include "AppUpdateProtocol.h"
#include "mqtt_client.h"


/////////////////////////////////////////////////////////////////////////////////////////
// ------ SubTopics, UnsubTopics
/////////////////////////////////////////////////////////////////////////////////////////
#define TOPIC_CACHE_CAPACITY            10

struct MqttSubTopic {
  const char *topic;
  uint8_t qos;
};

struct UnsubTopics
{
    void addUnsubTopic(const char *topic) {
        if (count < TOPIC_CACHE_CAPACITY) {
            topics[count++] = topic;
        }
    }

    void clear() {
        for (uint16_t i = 0; i < TOPIC_CACHE_CAPACITY; ++i) {
            topics[i] = NULL;
        }
        count = 0;
    }

    uint16_t        count;
    const char     *topics[TOPIC_CACHE_CAPACITY];
};

struct SubTopics
{
    void addSubTopic(const char *topic, uint8_t qos) {
        if (findTopic(topic) == -1 && count < TOPIC_CACHE_CAPACITY) {
            topics[count].topic = topic;
            topics[count].qos = qos;
            ++count;
        }
    }

    void addSubTopics(SubTopics &subTopics) {
        if (subTopics.count + count > TOPIC_CACHE_CAPACITY)
            return;
        for (uint16_t i = 0; i < subTopics.count; ++i) {
            if (findTopic(subTopics.topics[i].topic) == -1) {
                topics[count] = subTopics.topics[i];
                ++count;
            }
        }
    }

    void removeUnsubTopics(UnsubTopics &unsubTopics) {
        bool needCleanupNull = false;
        for (uint16_t i = 0; i < unsubTopics.count; ++i) {
            int index = findTopic(unsubTopics.topics[i]);
            if (index != -1) {
                topics[index].topic = NULL;
                needCleanupNull = true;
            }
        }
        if (needCleanupNull) cleanupNull();
    }

    void cleanupNull() {
        uint16_t searchCount = count;
        uint16_t j = 0;
        for (uint16_t i = 0; i < searchCount; ++i) {
            if (topics[i].topic == NULL) {
                --count;
                if (j == 0) j = i;
                while (++j < searchCount) {
                    if (topics[j].topic != NULL) {
                        topics[i].topic = topics[j].topic;
                        topics[i].qos = topics[j].qos;
                        topics[j].topic = NULL;
                        break;
                    }
                }
            }
        }
    }

    int findTopic(const char *topic) {
        for (uint16_t i = 0; i < count; ++i) {
            if (topics[i].topic != NULL && strcmp(topics[i].topic, topic) == 0)
                return i;
        }
        return -1;
    }

    void clear() {
        for (uint16_t i = 0; i < TOPIC_CACHE_CAPACITY; ++i) {
            topics[i].topic = NULL;
            topics[i].qos = 0;
        }
        count = 0;
    }

    uint16_t        count;
    MqttSubTopic    topics[TOPIC_CACHE_CAPACITY];
};


/////////////////////////////////////////////////////////////////////////////////////////
// ------ MqttClient class
/////////////////////////////////////////////////////////////////////////////////////////
#define MONGOOSE_MQTT_DEFAULT_POLL_SLEEP     0

class MqttClient : public MessagePubDelegate, public MqttClientDelegate,
                   public AppUpdateFirmwareFetchClient, AppUpdateRetCodeHandler
{
public:
    // --- constructor
    MqttClient();

    // --- config connection
    void setServerAddress(const char* serverAddress);
    void setAliveGuardInterval(time_t interval) { _aliveGuardInterval = interval; }
    time_t aliveGuardInterval() { return _aliveGuardInterval; }
    void setOnServerUnavailableReconnectDelay(TickType_t delayTicks);
    void setOnDisconnectedReconnectDelay(TickType_t delayTicks);
    void setSubscribeImmediateOnConnected(bool immediate = true);

    // --- config mqtt client connection protocol
    void setClientId(const char *clientId);
    void setCleanSession(bool cleanSenssion = true);
    void setUserPassword(const char *user, const char *password);
    void setKeepAlive(uint16_t value);
    void setLastWill(const char* topic, uint8_t qos, const char* msg, bool retain);

    // init and deinit
    void init();
    void deinit();

    // connection
    void start();
    bool connected() { return _connected; }

    const SubTopics & topicsSubscribed();

protected:
    bool _makeConnection();
    void _closeProcess();

public:
    // --- MqttClientDelegate interface
    virtual void addSubTopic(const char *topic, uint8_t qos = 0);
    virtual void subscribeTopics();
    virtual bool hasTopicsToSubscribe() { return _topicsToSubscribe.count > 0; }

    virtual void addUnsubTopic(const char *topic);
    virtual void unsubscribeTopics();
    virtual bool hasTopicsToUnsubscribe() { return _topicsToUnsubscribe.count > 0; }

    virtual void publish(const char *topic,
                         const void *data,
                         size_t      len,
                         uint8_t     qos,
                         bool        retain = false,
                         bool        dup = false);
    virtual bool hasUnackPub();

    // --- MessagePubDelegate interface
    virtual void repubMessage(PoolMessage *message);

    // --- AppUpdateFirmwareFetchClient interface
    virtual void initUpdater();
    virtual bool sendUpdateRequest();
    virtual bool sendData(const void *data, size_t len);
    // virtual void onRxData(const void *data, size_t len);

    virtual void onUpdateEnded(bool succeeded = true,  bool stopDelegate = false);
    virtual int customizedFun(const void *data);

    // --- AppUpdateRetCodeHandler interface
    virtual void handleUpdateRetCode(int code, const char *msg, int value);

    // -----------------------------------------
    // for alive guard check task
    // Note: keep alive is implemented in esp lib
    // -----------------------------------------
    // void aliveGuardCheck();

public:
    // --- for event handler
    void onConnect(esp_mqtt_event_t *e);
    void onConnAct(esp_mqtt_event_t *e);
    void onPubAck(esp_mqtt_event_t *e);
    void onSubAck(esp_mqtt_event_t *e);
    void onUnsubAct(esp_mqtt_event_t *e);
    void onRxPubMessage(esp_mqtt_event_t *e);
    void onClose(esp_mqtt_event_t *e);

    // these protocol actions unavailable at esp application level
    void onPubRec(esp_mqtt_event_t *e);
    void onPubComp(esp_mqtt_event_t *e);
    void onPingResp(esp_mqtt_event_t *e);
    void onTimeout(esp_mqtt_event_t *e);

protected:
    // init and connection
    bool                                _inited;
    bool                                _connected;
    bool                                _subscribeImmediatelyOnConnected;
    TickType_t                          _reconnectTicksOnServerUnavailable;
    TickType_t                          _reconnectTicksOnDisconnection;
    time_t                              _aliveGuardInterval;
    time_t                              _recentActiveTime;

    // esp mqtt
    esp_mqtt_client_config_t            _config;
    esp_mqtt_client                    *_client;        

    // mqtt topics
    bool                                _subscribeInProgress;
    bool                                _unsubscribeInProgress;
    SubTopics                           _topicsSubscribed;
    SubTopics                           _topicsToSubscribe;
    UnsubTopics                         _topicsToUnsubscribe;

    // message publish pool
    MessagePubPool                      _msgPubPool;
};

#endif // _MQTT_CLIENT_H
