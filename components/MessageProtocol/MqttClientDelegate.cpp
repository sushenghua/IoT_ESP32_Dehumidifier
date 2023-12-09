/*
 * MqttClientDelegate: interface for communication with MQTT Client object
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "MqttClientDelegate.h"

#include <string.h>

#include "System.h"

#define TOPIC_API_CMD_HEAD           "api/cmd/"
#define TOPIC_API_STR_CMD_HEAD       "api/strcmd/"

#define CMD_RET_MSG_TOPIC_HEAD       "api/cmdret/"
#define PUB_MSG_QOS                  1

static char _cmdTopic[32];
static char _strCmdTopic[32];
static char _cmdRetTopic[32];

const char * MqttClientDelegate::cmdTopic()
{
    return _cmdTopic;
}

const char * MqttClientDelegate::strCmdTopic()
{
    return _strCmdTopic;
}

const char * MqttClientDelegate::cmdRetTopic()
{
    return _cmdRetTopic;
}

inline void catenateTopic(char * target, const char * head, const char * tail)
{
    strcpy(target, head);
    strcat(target, tail);
}

void MqttClientDelegate::setup()
{
    const char * uid = System::instance()->uid();

    catenateTopic(_cmdTopic, TOPIC_API_CMD_HEAD, uid);
    catenateTopic(_strCmdTopic, TOPIC_API_STR_CMD_HEAD, uid);
    catenateTopic(_cmdRetTopic, CMD_RET_MSG_TOPIC_HEAD, uid);

    this->addSubTopic(_cmdTopic);
    this->addSubTopic(_strCmdTopic);
    this->subscribeTopics();
}

void MqttClientDelegate::replyMessage(const void *data, size_t length, void *userdata, int type)
{
    (void)userdata;
    (void)type;
    this->publish(_cmdRetTopic, data, length, PUB_MSG_QOS);
}
