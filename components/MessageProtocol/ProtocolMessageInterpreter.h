/*
 * ProtocolMessageInterpreter: communication message interpreter
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _MQTT_MESSAGE_INTERPRETER_H
#define _MQTT_MESSAGE_INTERPRETER_H

#include <stddef.h>

#define MSG_SRC_WS_CLIENT       1
#define MSG_SRC_HTTP_SERVER     2

class ProtocolMessageInterpreter
{
public:
    // virtual interface
    virtual void interpretMqttMsg(const char* topic, size_t topicLen, const char* msg, size_t msgLen) = 0;
    virtual void interpretSocketMsg(const void* msg, size_t msgLen, int src, int type, void *userdata) = 0;
};

#endif // _MQTT_MESSAGE_INTERPRETER_H