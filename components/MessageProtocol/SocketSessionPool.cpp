/*
 * SocketSessionPool: management of live socket sessions
 * Copyright (c) 2023 Shenghua Su
 *
 */

#include "SocketSessionPool.h"
#include "AppLog.h"
// #include "lwip/sockets.h"
#include <algorithm>

SocketSessionPool::SocketSessionPool()
{
  _initFreeSlotStack();
}

void SocketSessionPool::_initFreeSlotStack()
{
  for (int i = 0; i < SESSION_POOL_CAPACITY; ++i) {
    _freeSlotStack[i] = i;
  }
  _stackTopIndex = 0;
}

inline bool SocketSessionPool::_hasFreeSlot()
{
  return _stackTopIndex < SESSION_POOL_CAPACITY - 1;
}

inline int SocketSessionPool::_popFreeSlot()
{
  if (_stackTopIndex < SESSION_POOL_CAPACITY - 1) {
    return _freeSlotStack[_stackTopIndex++];
  } else {
    return -1;
  }
}

inline bool SocketSessionPool::_pushFreeSlot(int slot)
{
  if (_stackTopIndex > 0) {
    _freeSlotStack[--_stackTopIndex] = slot;
    return true;
  } else {
    return false;
  }
}

bool SocketSessionPool::addSession(int socketFd)
{
  if (_fdMap.find(socketFd) != _fdMap.end()) return false;
  if (!_hasFreeSlot()) return false;

  int slot = _popFreeSlot();
  _fdMap[socketFd] = slot;
  return true;
}

void SocketSessionPool::drainSession(int socketFd)
{
  std::map<int, int>::iterator it = _fdMap.find(socketFd);
  if (it != _fdMap.end()) {
    int slot = _fdMap[socketFd];
    _pushFreeSlot(slot);
    _fdMap.erase(it);
  }
}

void SocketSessionPool::clean()
{
  _fdMap.clear();
  _initFreeSlotStack();
}

size_t SocketSessionPool::sessionCount()
{
  return _fdMap.size();
}

void SocketSessionPool::broadcastMessage(httpd_handle_t hd, const char *data, size_t len)
{
  if (_fdMap.size() == 0) return;

  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.payload = (uint8_t*)data;
  ws_pkt.len = len;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;

  std::for_each(_fdMap.begin(), _fdMap.end(), [&](std::pair<int, int> element) {
    int fd = element.first;
    esp_err_t ret = httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    if (ret != ESP_OK) APP_LOGE("[SocketPool]", "httpd_ws_send_frame_async failed with %d", ret);
  });
}
