/*
 * MessagePubPool: managment of un-acknowledged message
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _MESSAGE_PUB_POOL_H
#define _MESSAGE_PUB_POOL_H

#include "freertos/FreeRTOS.h"
#include <time.h>
#include <map>

/////////////////////////////////////////////////////////////////////////////////////////
// ------ PoolMessage class
/////////////////////////////////////////////////////////////////////////////////////////
class PoolMessage
{
public:
    int           msgId;
    const char   *topic;
    const void   *data;
    size_t        length;
    uint8_t       qos;
    bool          retain;
    uint16_t      pubCount;
    time_t        rebornTime;   // seconds
};


/////////////////////////////////////////////////////////////////////////////////////////
// ------ MessagePubDelegate class
/////////////////////////////////////////////////////////////////////////////////////////
class MessagePubDelegate
{
public:
    virtual void repubMessage(PoolMessage *message) = 0;
};


/////////////////////////////////////////////////////////////////////////////////////////
// ------ MessagePubPool class
/////////////////////////////////////////////////////////////////////////////////////////
#define PUB_POOL_CAPACITY                       10
#define POOL_PROCESS_LOOP_DEFAULT_INTERVAL      5000

class MessagePubPool
{
public:
    MessagePubPool(TickType_t loopInterval = POOL_PROCESS_LOOP_DEFAULT_INTERVAL);

    TickType_t loopInterval() { return _loopInterval; }
    void setLoopInterval(TickType_t interval) { _loopInterval = interval; }
    void setMessageCheckDueDuration(uint16_t duration) { _messageCheckDueDuration = duration; }
    void setPubDelegate(MessagePubDelegate *delegate);

    // task loop
    void processLoop();

    // used by controller
    bool addMessage(int         msgId,
                    const char* topic,
                    const void* data,
                    size_t      len,
                    uint8_t     qos,
                    bool        retain);
    void drainPoolMessage(int msgId);
    void cleanPool();
    size_t poolMessageCount();

protected:
    // repub helper
    PoolMessage * _message(int msgId);
    void _repubPoolMessage(int msgId);

protected:
    // free slot helper
    void _initFreeSlotStack();
    bool _hasFreeSlot();
    int  _popFreeSlot();
    bool _pushFreeSlot(int slot);

protected:
	// message alive check due duration
	uint16_t                    _messageCheckDueDuration;
	// loop interval
	TickType_t                  _loopInterval;
    // delegate
    MessagePubDelegate         *_delegate;
    // message free slot stack
    int                         _freeSlotStack[PUB_POOL_CAPACITY];
    int                         _stackTopIndex;
    // message map
    std::map<int, int>          _messageMap;
    // message content buf
    PoolMessage                 _messageBuf[PUB_POOL_CAPACITY];
};

#endif // _MESSAGE_PUB_POOL_H
