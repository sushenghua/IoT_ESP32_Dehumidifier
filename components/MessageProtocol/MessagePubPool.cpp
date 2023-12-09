/*
 * MessagePubPool: managment of un-acknowledged message
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "MessagePubPool.h"
#include "AppLog.h"
#include <algorithm>

PoolMessage * MessagePubPool::_message(int msgId)
{
    std::map<int, int>::iterator it = _messageMap.find(msgId);
    if (it != _messageMap.end()) {
        return &_messageBuf[_messageMap[msgId]];
    }
    return NULL;
}

void MessagePubPool::_repubPoolMessage(int msgId)
{
    PoolMessage *message = _message(msgId);
    if (message && _delegate) {
        _delegate->repubMessage(message);
    }
}

void MessagePubPool::_initFreeSlotStack()
{
    for (int i = 0; i < PUB_POOL_CAPACITY; ++i) {
        _freeSlotStack[i] = i;
    }
    _stackTopIndex = 0;
}

inline bool MessagePubPool::_hasFreeSlot()
{
    return _stackTopIndex < PUB_POOL_CAPACITY - 1;
}

inline int MessagePubPool::_popFreeSlot()
{
    if (_stackTopIndex < PUB_POOL_CAPACITY - 1) {
        return _freeSlotStack[_stackTopIndex++];
    }
    return -1;
}

inline bool MessagePubPool::_pushFreeSlot(int slot)
{
    if (_stackTopIndex > 0) {
        _freeSlotStack[--_stackTopIndex] = slot;
        return true;
    }
    return false;
}

#define MESSAGE_CHECK_DUE_DURATION_DEFAULT     5

MessagePubPool::MessagePubPool(TickType_t loopInterval)
: _messageCheckDueDuration(MESSAGE_CHECK_DUE_DURATION_DEFAULT)
, _loopInterval(loopInterval)
, _delegate(NULL)
{
	_initFreeSlotStack();
}

void MessagePubPool::setPubDelegate(MessagePubDelegate *delegate)
{
    _delegate = delegate;
}

void MessagePubPool::processLoop()
{
    //APP_LOGI("[MessagePubPool]", "porcess loop");
    time_t timeNow = time(NULL);
    // std::map<uint16_t, int>::iterator it = _messageMap.begin();
    // while (it != _messageMap.end()) {
    //     if (timeNow - _messageBuf[element.second].rebornTime > _messageCheckDueDuration) {
    //         // repub
    //         if (_delegate) {
    //             _delegate->repubMessage(&_messageBuf[it->second]);
    //             _messageBuf[it->second].pubCount++;
    //             _messageBuf[it->second].rebornTime = timeNow;
    //         }
    //     }
    // }
    std::for_each(_messageMap.begin(), _messageMap.end(), [&](std::pair<uint16_t, int> element) {
        if (timeNow - _messageBuf[element.second].rebornTime > _messageCheckDueDuration) {
            if (_delegate) {
                _delegate->repubMessage(&_messageBuf[element.second]);
                _messageBuf[element.second].pubCount++;
                _messageBuf[element.second].rebornTime = timeNow;
            }
        }
    });
}

bool MessagePubPool::addMessage(int         msgId,
                                const char* topic,
                                const void* data,
                                size_t      len,
                                uint8_t     qos,
                                bool        retain)
{
    if (_messageMap.find(msgId) != _messageMap.end()) {
        return false;
    }

    if (_hasFreeSlot()) {
        int slot = _popFreeSlot();
        // add to map
        _messageMap[msgId] = slot;
        // save message
        _messageBuf[slot].msgId = msgId;
        _messageBuf[slot].topic = topic;
        _messageBuf[slot].data = data;
        _messageBuf[slot].length = len;
        _messageBuf[slot].qos = qos;
        _messageBuf[slot].retain = retain;
        _messageBuf[slot].pubCount = 1;
        _messageBuf[slot].rebornTime = time(NULL);
        return true;
    }

    return false;
}

void MessagePubPool::drainPoolMessage(int msgId)
{
    std::map<int, int>::iterator it = _messageMap.find(msgId);
    if (it != _messageMap.end()) {
        int slot = _messageMap[msgId];
        _pushFreeSlot(slot);
        _messageMap.erase(it);
    }
}

void MessagePubPool::cleanPool()
{
    _messageMap.clear();
    _initFreeSlotStack();
}

size_t MessagePubPool::poolMessageCount()
{
    return _messageMap.size();
}
