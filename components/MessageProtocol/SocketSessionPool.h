/*
 * SocketSessionPool: management of live socket sessions
 * Copyright (c) 2023 Shenghua Su
 *
 */

#ifndef _SOCKET_SESSION_POOL_H
#define _SOCKET_SESSION_POOL_H

#include <time.h>
#include <map>
#include "esp_http_server.h"

/////////////////////////////////////////////////////////////////////////////////////////
// ------ SocketSessionPool class
/////////////////////////////////////////////////////////////////////////////////////////
#define SESSION_POOL_CAPACITY           5

class SocketSessionPool
{
public:
  // --- constructor
  SocketSessionPool();

  // --- interface
  bool addSession(int socketFd);
  void drainSession(int socketFd);
  void clean();
  size_t sessionCount();
  void broadcastMessage(httpd_handle_t hd, const char *data, size_t len);

protected:
  // --- free slot helper
  void _initFreeSlotStack();
  bool _hasFreeSlot();
  int  _popFreeSlot();
  bool _pushFreeSlot(int slot);

protected:
  // --- free slot stack
  int                         _freeSlotStack[SESSION_POOL_CAPACITY];
  int                         _stackTopIndex;
  // --- socket fd map
  std::map<int, int>          _fdMap;
};

#endif // _SOCKET_SESSION_POOL_H
