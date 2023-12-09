/*
 * CmdEngine command interpretation
 * Copyright (c) 2016 Shenghua Su
 *
 */

#ifndef _CMD_ENGINE_H
#define _CMD_ENGINE_H

#include "ProtocolMessageInterpreter.h"
#include "ProtocolDelegate.h"
#include "AppUpdateProtocol.h"
#include "CmdKey.h"

class CmdEngine : public ProtocolMessageInterpreter
{
public:
  enum RetFormat {
    Binary,
    JSON,
  };

public:
  CmdEngine();

  bool init();

  void enableUpdate(AppUpdateFirmwareFetchClient *updateDelegate, AppUpdateRetCodeHandler *codeHandler);

  int execCmd(CmdKey cmdKey, RetFormat retFmt = Binary, uint8_t *args = NULL, size_t argsSize = 0, void *userdata = NULL);

  void setProtocolDelegate(ProtocolDelegate *delegate);

  // --- ProtocolMessageInterpreter interface
  virtual void interpretMqttMsg(const char* topic, size_t topicLen, const char* msg, size_t msgLen);
  virtual void interpretSocketMsg(const void* msg, size_t msgLen, int src, int type, void *userdata);

protected:
  bool                   _updateEnabled;
  ProtocolDelegate      *_delegate;
};

#endif // _CMD_ENGINE_H
