/*
 * ProtocolDelegate: interface for communication with protocol object
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _PROTOCOL_DELEGATE_H
#define _PROTOCOL_DELEGATE_H

#include <stdint.h>
#include "ProtocolMessageInterpreter.h"

#define PROTOCOL_MSG_FORMAT_TEXT    1
#define PROTOCOL_MSG_FORMAT_BINARY  2

class ProtocolDelegate
{
public:
    // constructor
    ProtocolDelegate(): _msgInterpreter(NULL) {}
    // message interpreter
    void setMessageInterpreter(ProtocolMessageInterpreter *interpreter) {
        _msgInterpreter = interpreter;
    }
    // virtual functions
    virtual void setup() = 0;
    virtual void replyMessage(const void *data,
                              size_t      length,
                              void       *userdata = NULL,
                              int         type = PROTOCOL_MSG_FORMAT_TEXT) = 0;

protected:
    ProtocolMessageInterpreter *_msgInterpreter;
};

#endif // _PROTOCOL_DELEGATE_H
