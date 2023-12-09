/*
 * CmdEngine command interpretation
 * Copyright (c) 2016 Shenghua Su
 *
 */

#include "CmdEngine.h"

#include <string.h>
#include "CmdFormat.h"
#include "AppLog.h"
#include "System.h"
#include "SensorDataPacker.h"
#include "SharedBuffer.h"
#include "AppUpdater.h"
#include "MqttClientDelegate.h"
#include "Wifi.h"
#include "Config.h"
#include "SensorConfig.h"

#include "cJSON.h"

#define TAG "[CmdEngine]"

AppUpdater _appUpdater;
char    *_strBuf = NULL;
uint8_t *_cmdBuf = NULL;

CmdEngine::CmdEngine()
: _updateEnabled(false)
, _delegate(NULL)
{}

void CmdEngine::setProtocolDelegate(ProtocolDelegate *delegate)
{
  _delegate = delegate;
  _delegate->setMessageInterpreter(this);
}

bool CmdEngine::init()
{
  bool succeeded = false;
  if (_delegate) {
    _delegate->setup();
    succeeded = true;
  }
  _strBuf = SharedBuffer::msgBuffer();
  _cmdBuf = SharedBuffer::cmdBuffer();
  return succeeded;
}

void CmdEngine::enableUpdate(AppUpdateFirmwareFetchClient *updateDelegate, AppUpdateRetCodeHandler *codeHandler)
{
  if (!_updateEnabled) {
    _updateEnabled = true;
    updateDelegate->setMessageInterpreter(this);
    _appUpdater.setUpdateDelegate(updateDelegate, codeHandler);
  }
}

static const char * const SensorTypeParseKeyStr[] = { "pm", "co2" };
static const char * const AlertEnableParseKeyStr[] = { "enpn", "ensnd"};

const size_t FloatLen = sizeof(float);
union FloatBytes {
  float v;
  uint8_t bytes[FloatLen];
};

inline bool strEqual(const char* str1, const char* str2)
{
  return strlen(str1) == strlen(str2) && strcmp(str1, str2) == 0;
}

CmdKey _parseJsonStringCmd(const char* msg, size_t msgLen, uint8_t *&args, size_t &argsSize, CmdEngine::RetFormat &retFmt)
{
  cJSON *root = cJSON_Parse(msg);
  cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
#ifdef LOG_REMOTE_CMD
  if (!cmd) APP_LOGE("[CmdEngine]", "json cmd parse err, raw msg len: %d, msg: %.*s", msgLen, msgLen, msg);
  // else APP_LOGC("[CmdEngine]", "json cmd %s", cmd->valuestring);
#endif
  if (!cmd) return DoNothing;

  CmdKey cmdKey = strToCmdKey(cmd->valuestring);

  cJSON *retfmt = cJSON_GetObjectItem(root, "retfmt");
  if (retfmt && strcmp("json", retfmt->valuestring) == 0) retFmt = CmdEngine::JSON;
  else retFmt = CmdEngine::Binary;

  CmdKey cmdKeyRet = DoNothing;
  args = _cmdBuf;

  switch (cmdKey) {

    case SetFanOpMode: {
      bool parseOK = true;
      FloatBytes fb;
      cJSON *mode = cJSON_GetObjectItem(root, "mode");
      if (mode) {
        if (strEqual("auto", mode->valuestring)) {
          memset(args, FAN_MODE_AUTO, 1);
          cJSON *lThreshold = cJSON_GetObjectItem(root, "lthr");
          cJSON *gThreshold = cJSON_GetObjectItem(root, "gthr");
          fb.v = (lThreshold) ? (float)lThreshold->valuedouble : 0.0f;
          memcpy(args + 1,            fb.bytes, FloatLen);
          fb.v = (gThreshold) ? (float)gThreshold->valuedouble : 0.0f;
          memcpy(args + 1 + FloatLen, fb.bytes, FloatLen);
          argsSize = FloatLen * 2 + 1;
        }
        else if (strEqual("delay", mode->valuestring)) {
          memset(args, FAN_MODE_DELAY, 1);
          cJSON *min = cJSON_GetObjectItem(root, "min");
          fb.v = (min) ? (float)min->valuedouble : 0.0f;
          memcpy(args + 1, fb.bytes, FloatLen);
          argsSize = FloatLen + 1;
        }
        else if (strEqual("on", mode->valuestring)) {
          memset(args, FAN_MODE_ON, 1);
          argsSize = 1;
        }
        else if (strEqual("off", mode->valuestring)) {
          memset(args, FAN_MODE_OFF, 1);
          argsSize = 1;
        }
        else { parseOK = false; }
      }  else { parseOK = false; }
      if (parseOK) cmdKeyRet = cmdKey;
      break;
    }

    case SetHostname:
    case SetDeviceName: {
      cJSON *name = cJSON_GetObjectItem(root, "name");
      if (name) {
        argsSize = strlen(name->valuestring);
        memcpy(args, name->valuestring, argsSize);
        cmdKeyRet = cmdKey;
      }
      break;
    }

    case SetStaSsidPass:
    case SetApSsidPass:
    case AppendAltApSsidPass: {
      cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
      cJSON *pass = cJSON_GetObjectItem(root, "pass");
      if (ssid && pass) {
        // cJSON *sih = cJSON_GetObjectItem(root, "sih");
        // cJSON *sil = cJSON_GetObjectItem(root, "sil");
        // if (sih && sil && sih->type == cJSON_Number && sil->type == cJSON_Number) {
        //   args[0] = 1;
        //   uint32_t hv = (uint32_t)sih->valuedouble;
        //   uint32_t lv = (uint32_t)sil->valuedouble;
        //   memcpy(args + 1, &hv, 4);
        //   memcpy(args + 5, &lv, 4);
        // }
        // else {
        //   args[0] = 0;
        // }
        size_t offset = 9;
        args[offset] = strlen(ssid->valuestring);
        size_t passLen = strlen(pass->valuestring);
        memcpy(args + offset + 1,                ssid->valuestring, args[offset]);
        memcpy(args + offset + 1 + args[offset], pass->valuestring, passLen);
        argsSize = offset + 1 + args[offset] + passLen;
        cmdKeyRet = cmdKey;
        // APP_LOGC("[CmdEngine]", "setSta: %.*s, ssidLen: %d", argsSize-1, args+1, args[0]);
      }
      break;
    }

    case SetSystemDeployMode: {
      cJSON *mode = cJSON_GetObjectItem(root, "mode");
      if (mode) {
        argsSize = 1;
        for (int i=0; i<DeployModeMax; ++i) {
          if (strEqual(mode->valuestring, deployModeStr((DeployMode)i))) {
            args[0] = i;
            cmdKeyRet = cmdKey;
            break;
          }
        }
      }
      break;
    }

    case SetSensorType: {
      argsSize = 0;
      for (int oi=0; oi<2; ++oi) {
        cJSON *obj = cJSON_GetObjectItem(root, SensorTypeParseKeyStr[oi]);
        if (!obj) break;
        for (uint8_t i=0; i<SensorTypeMax; ++i) {
          if (strEqual(obj->valuestring, sensorTypeStr((SensorType)i))) {
            args[oi] = i;
            ++argsSize;
            break;
          }
        }
      }
      if (argsSize == 2) cmdKeyRet = cmdKey;
      break;
    }

    case TurnOnDisplay:
    case TurnOnAutoAdjustDisplay: {
      cJSON *on = cJSON_GetObjectItem(root, "on");
      if (!on) break;
      if (on->type == cJSON_True)       args[0] = 1;
      else if (on->type == cJSON_False) args[0] = 0;
      else break;

      argsSize = 1;
      cmdKeyRet = cmdKey;
      break;
    }

    case SetAlertEnableConfig: {
      argsSize = 2;
      cJSON *config = cJSON_GetObjectItem(root, "config");
      if (config) {
        for (int oi=0; oi<2; ++oi) {
          cJSON *obj = cJSON_GetObjectItem(config, AlertEnableParseKeyStr[oi]);
          if (!obj) { argsSize = 0; break; }
          if (obj->type == cJSON_True)       args[oi] = 1;
          else if (obj->type == cJSON_False) args[oi] = 0;
          else { argsSize = 0; break; }
        }
      } else { argsSize = 0; }
      if (argsSize == 2) cmdKeyRet = cmdKey;
      break;
    }

    case SetSensorAlertConfig: {
      argsSize = 0;
      bool parseOK = true;
      FloatBytes fb;
      size_t bLen = FloatLen * 2 + 2;
      cJSON *config = cJSON_GetObjectItem(root, "config");
      if (config) {
        for (int i=0; i<SensorDataTypeCount; ++i) {
          SensorDataType sdt = (SensorDataType)i;
          const char* sdtstr = sensorDataTypeStr(sdt);
          cJSON *alerts = cJSON_GetObjectItem(config, sdtstr);
          if (!alerts) { parseOK = false; break; }
          cJSON *len = cJSON_GetObjectItem(alerts, "len");
          cJSON *gen = cJSON_GetObjectItem(alerts, "gen");
          cJSON *lval = cJSON_GetObjectItem(alerts, "lval");
          cJSON *gval = cJSON_GetObjectItem(alerts, "gval");
          // if (!len || !gen || !lval || !gval) { parseOK = false; break; }
          args[i*bLen+0] = (len && len->type == cJSON_True) ? 1 : 0;
          args[i*bLen+1] = (gen && gen->type == cJSON_True) ? 1 : 0;
          fb.v = (lval) ? (float)lval->valuedouble : 0.0f;
          memcpy(args+i*bLen+2, fb.bytes, FloatLen);
          fb.v = (gval) ? (float)gval->valuedouble : 0.0f;
          memcpy(args+i*bLen+2+FloatLen, fb.bytes, FloatLen);
          argsSize += bLen;
        }
      } else { parseOK = false; }
      if (parseOK) cmdKeyRet = cmdKey;
      break;
    }

    case SetPNToken: {
      cJSON *en = cJSON_GetObjectItem(root, "en");
      if (!en) break;
      if (en->type == cJSON_True)       args[0] = 1;
      else if (en->type == cJSON_False) args[0] = 0;
      else break;

      cJSON *os = cJSON_GetObjectItem(root, "os");
      if (!os) break;
      if (strEqual(os->valuestring, "ios"))          args[1] = iOS;
      else if (strEqual(os->valuestring, "android")) args[1] = Android;
      else break;

      cJSON *token = cJSON_GetObjectItem(root, "token");
      if (token && TOKEN_LEN == strlen(token->valuestring)) {
        memcpy(args+2, token->valuestring, TOKEN_LEN);
        args[2+TOKEN_LEN] = '\0'; // null terminated
        argsSize = 3 + TOKEN_LEN;
        cmdKeyRet = cmdKey;
      }
      break;
    }

    case CheckPNTokenEnabled: {
      cJSON *os = cJSON_GetObjectItem(root, "os");
      if (!os) break;
      if (strEqual(os->valuestring, "ios"))          args[0] = iOS;
      else if (strEqual(os->valuestring, "android")) args[0] = Android;
      else break;

      cJSON *token = cJSON_GetObjectItem(root, "token");
      if (token && TOKEN_LEN == strlen(token->valuestring)) {
        memcpy(args + 1, token->valuestring, TOKEN_LEN);
        args[1+TOKEN_LEN] = '\0'; // null terminated
        argsSize = 2 + TOKEN_LEN;
        cmdKeyRet = cmdKey;
      }
      break;
    }

    case SetDebugFlag: {
      cJSON *flag = cJSON_GetObjectItem(root, "flag");
      if (flag && flag->type == cJSON_Number) {
        args[0] = flag->valueint;
        argsSize = 1;
        cmdKeyRet = cmdKey;
      }
      break;
    }

    default:
      cmdKeyRet = cmdKey;
      break;
  }

  cJSON_Delete(root);

  return cmdKeyRet;
}

void appendCmdKeyToJsonString(CmdKey cmdKey, char *jsonStr, size_t &count)
{
  sprintf(jsonStr + count - 1, ",\"cmd\":\"%s\"}", cmdKeyToStr(cmdKey));
  count += strlen(jsonStr + count);
}

void CmdEngine::interpretMqttMsg(const char* topic, size_t topicLen, const char* msg, size_t msgLen)
{
  if (_appUpdater.isUpdating()) {
    // app update topic
    // if (strncmp(topic, _appUpdater.updateRxTopic(), _appUpdater.updateRxTopicLen()) == 0) {
    //   _appUpdater.updateLoop(msg, msgLen);
    // }
    if (_appUpdater.customizedFun(topic)) {
      _appUpdater.updateLoop(msg, msgLen);
    }
  }
  else {
    bool exec = false;
    CmdKey cmdKey;
    uint8_t *data = NULL;
    size_t size = 0;
    RetFormat retFmt = Binary;

    // binary data command topic
    if (topicLen == strlen(MqttClientDelegate::cmdTopic()) &&
        strncmp(topic, MqttClientDelegate::cmdTopic(), topicLen) == 0) {
      // data size check
      if (msgLen >= CMD_DATA_AT_LEAST_SIZE) {
        data = (uint8_t *)msg;
        cmdKey = (CmdKey)( *(uint16_t *)(data + CMD_DATA_KEY_OFFSET) );
        data += CMD_DATA_ARG_OFFSET;
        size = msgLen - CMD_DATA_KEY_SIZE;
        retFmt = Binary;
        exec = true;
      }
      else {
        APP_LOGE("[CmdEngine]", "mqtt msg data size must be at least %d", CMD_DATA_AT_LEAST_SIZE);
      }
    }
    // string data command topic
    else if (topicLen == strlen(MqttClientDelegate::strCmdTopic()) &&
        strncmp(topic, MqttClientDelegate::strCmdTopic(), topicLen) == 0) {
      cmdKey = _parseJsonStringCmd(msg, msgLen, data, size, retFmt);
      exec = true;
    }

    if (exec) execCmd(cmdKey, retFmt, data, size);
  }
}

void CmdEngine::interpretSocketMsg(const void* msg, size_t msgLen, int src, int type, void *userdata)
{
  if (src == MSG_SRC_WS_CLIENT && _appUpdater.isUpdating()) {
    _appUpdater.updateLoop((const char *)msg, msgLen);
  }
  else {
    bool exec = false;
    CmdKey cmdKey;
    uint8_t *data = NULL;
    size_t size = 0;
    RetFormat retFmt = Binary;

    if (type == PROTOCOL_MSG_FORMAT_BINARY) {
      if (msgLen >= CMD_DATA_AT_LEAST_SIZE) {
        data = (uint8_t *)msg;
        cmdKey = (CmdKey)( *(uint16_t *)(data + CMD_DATA_KEY_OFFSET) );
        data += CMD_DATA_ARG_OFFSET;
        size = msgLen - CMD_DATA_KEY_SIZE;
        exec = true;
      }
      else {
        APP_LOGE(TAG, "socket msg data size must be at least %d", CMD_DATA_AT_LEAST_SIZE);
      }
    }
    else if (type == PROTOCOL_MSG_FORMAT_TEXT) {
      cmdKey = _parseJsonStringCmd((const char *)msg, msgLen, data, size, retFmt);
      exec = true;
    }

    if (exec) execCmd(cmdKey, retFmt, data, size, userdata);
  }
}

void replyJsonResult(ProtocolDelegate *delegate, const char *str, CmdKey cmdKey, void *userdata)
{
  sprintf(_strBuf, "{\"ret\":\"%s\", \"cmd\":\"%s\"}", str, cmdKeyToStr(cmdKey));
  delegate->replyMessage(_strBuf, strlen(_strBuf), userdata);
}

int CmdEngine::execCmd(CmdKey cmdKey, RetFormat retFmt, uint8_t *args, size_t argsSize, void *userdata)
{
  switch (cmdKey) {

    case GetSensorData: {
      size_t count = 0;
      const void * data = NULL;
      if (retFmt == Binary)
        data = SensorDataPacker::sharedInstance()->dataBlock(count);
      else if (retFmt == JSON) {
        data = SensorDataPacker::sharedInstance()->dataJsonString(count);
        appendCmdKeyToJsonString(cmdKey, (char*)data, count);
      }
      _delegate->replyMessage(data, count, userdata);
      break;
    }

    case GetFanOpMode: {
      System *sys = System::instance();
      FanMode *m = sys->fanMode();
      char modestr[6];
      if (m->mode == FAN_MODE_AUTO)       sprintf(modestr, "auto");
      else if (m->mode == FAN_MODE_DELAY) sprintf(modestr, "delay");
      else if (m->mode == FAN_MODE_ON)    sprintf(modestr, "on");
      else if (m->mode == FAN_MODE_OFF)   sprintf(modestr, "off");
      sprintf(_strBuf, "{\"ret\":{\"mode\":\"%s\",\"lthr\":%.1f,\"gthr\":%.1f,\"min\":%.1f}, \"cmd\":\"%s\"}",
              modestr, m->lthr, m->gthr, m->min, cmdKeyToStr(cmdKey));
      _delegate->replyMessage(_strBuf, strlen(_strBuf), userdata);
      break;
    }

    case SetFanOpMode: {
      System *sys = System::instance();
      uint8_t mode = args[0];
      if (mode == FAN_MODE_AUTO) {
        FloatBytes lThr, gThr;
        memcpy(lThr.bytes, args + 1,            FloatLen);
        memcpy(gThr.bytes, args + 1 + FloatLen, FloatLen);
        sys->setFanMode(mode, lThr.v, gThr.v);
      }
      else if (mode == FAN_MODE_DELAY) {
        FloatBytes min;
        memcpy(min.bytes, args + 1, FloatLen);
        sys->setFanMode(mode, min.v, 0.f);
      }
      else if (mode == FAN_MODE_ON || mode == FAN_MODE_OFF) {
        sys->setFanMode(mode, 0.f, 0.f);
      }
      break;
    }

    case GetDeviceInfo:
      if (retFmt == JSON) {
        System *sys = System::instance();
        sprintf(_strBuf, "{\"ret\":{\"uid\":\"%s\",\"cap\":\"%lu\",\"libv\":\"%s\",\"firmv\":\"%s\",\"bdv\":\"%s\",\"model\":\"%s\",\"alcd\":%s,\"deploy\":\"%s\",\"hostname\":\"%s\",\"devname\":\"%s\",\"life\":\"%ld\"}, \"cmd\":\"%s\"}",
                sys->uid(),
                sys->devCapability(),
                sys->idfVersion(),
                sys->firmwareVersion(),
                sys->boardVersion(),
                sys->model(),
                sys->displayAutoAdjustOn()? "true" : "false",
                deployModeStr(sys->deployMode()),
                Wifi::instance()->getHostName(),
                sys->deviceName(),
                sys->maintenance()->allSessionsLife,
                cmdKeyToStr(cmdKey));
        _delegate->replyMessage(_strBuf, strlen(_strBuf), userdata);
      }
      break;

    case GetUID:
      if (retFmt == JSON) {
        replyJsonResult(_delegate, System::instance()->uid(), cmdKey, userdata);
      }
      else {
        _delegate->replyMessage(System::instance()->uid(), strlen(System::instance()->uid()), userdata);
      }
      break;

    case GetSensorCapability: {
      uint32_t capability = System::instance()->devCapability();
      if (retFmt == JSON) {
        sprintf(_strBuf, "{\"ret\":\"%lu\", \"cmd\":\"%s\"}", capability, cmdKeyToStr(cmdKey));
        _delegate->replyMessage(_strBuf, strlen(_strBuf), userdata);
      }
      else {
        _delegate->replyMessage(&capability, sizeof(capability), userdata);
      }
      break;
    }

    case GetIdfVersion:
      if (retFmt == JSON) {
        replyJsonResult(_delegate, System::instance()->idfVersion(), cmdKey, userdata);
      }
      else {
        _delegate->replyMessage(System::instance()->idfVersion(),
                                strlen(System::instance()->idfVersion()), userdata);
      }
      break;

    case GetFirmwareVersion:
      if (retFmt == JSON) {
        replyJsonResult(_delegate, System::instance()->firmwareVersion(), cmdKey, userdata);
      }
      else {
        _delegate->replyMessage(System::instance()->firmwareVersion(),
                                strlen(System::instance()->firmwareVersion()), userdata);
      }
      break;

    case GetHostname:
    case GetDeviceName: {
      const char *name = NULL;
      if (cmdKey == GetHostname) name = Wifi::instance()->getHostName();
      else if (cmdKey == GetDeviceName) name = System::instance()->deviceName();
      if (retFmt == JSON) {
        replyJsonResult(_delegate, name, cmdKey, userdata);
      }
      else {
        _delegate->replyMessage(name, strlen(name), userdata);
      }
      break;
    }

    case SetHostname:
      sprintf(_strBuf, "%.*s", argsSize, (const char*)args);
      Wifi::instance()->setHostName(_strBuf);
      Wifi::instance()->saveConfig();
      break;

    case SetDeviceName:
      System::instance()->setDeviceName((const char*)args, argsSize);
      break;

    case TurnOnDisplay:
      System::instance()->turnDisplayOn(args[0] != 0);
      break;

    case TurnOnAutoAdjustDisplay:
      System::instance()->turnDisplayAutoAdjustOn(args[0] != 0);
      break;

    case UpdateFirmware:
      if (!_appUpdater.isUpdating()) {
        _appUpdater.init();
        _appUpdater.update();
      }
      break;

    case Restart:
      System::instance()->setRestartRequest();
      break;

    case RestoreFactory:
      System::instance()->restoreFactory();
      break;

    case GetStaSsidPass:
    case GetApSsidPass: {
      const char *ssid = NULL;
      const char *pass = NULL;
      if (cmdKey == GetStaSsidPass) {
        ssid = Wifi::instance()->staSsid();
        pass = Wifi::instance()->staPassword();
      }
      else if (cmdKey == GetApSsidPass) {
        ssid = Wifi::instance()->apSsid();
        pass = Wifi::instance()->apPassword();
      }
      if (retFmt == JSON) {
        sprintf(_strBuf, "{\"ret\":{\"ssid\":\"%s\",\"pass\":\"%s\"}, \"cmd\":\"%s\"}",
                ssid, pass, cmdKeyToStr(cmdKey));
        _delegate->replyMessage(_strBuf, strlen(_strBuf), userdata);
      }
      else {
        _strBuf[0] = strlen(ssid);
        sprintf(_strBuf + 1, "%s%s", ssid, pass);
        _delegate->replyMessage(_strBuf, 1 + strlen(_strBuf + 1), userdata);
      }
      break;
    }

    case GetAltApSsidPassList: {
      SsidPasswd *list = NULL;
      uint8_t head, count;
      Wifi::instance()->getAltApConnectionSsidPassword(list, head, count);
      if (retFmt == JSON) {
        size_t offset = 0;
        sprintf(_strBuf + offset, "{\"cmd\":\"%s\",\"ret\":[", cmdKeyToStr(cmdKey));
        offset += strlen(_strBuf + offset);
        uint8_t index = 0;
        for (uint8_t i = 0; i < count; ++i) {
          index = (head + i) % count;
          sprintf(_strBuf + offset, "{\"ssid\":\"%s\",\"pass\":\"%s\"}%s",
                  list[index].ssid, list[index].password, (i<count-1)?",":"");
          offset += strlen(_strBuf + offset);
        }
        sprintf(_strBuf + offset, "]}");
        offset += strlen(_strBuf + offset);
        _delegate->replyMessage(_strBuf, offset, userdata);
      }
      break;
    }

    case SetStaSsidPass:
    case SetApSsidPass:
    case AppendAltApSsidPass: {
      uint8_t offset = 9;
      uint8_t ssidLen = args[offset];
      char ssid[32] = {0};
      char pass[64] = {0};
      strncat(ssid, (const char*)(args + offset + 1),           ssidLen);
      strncat(pass, (const char*)(args + offset + 1 + ssidLen), argsSize - 1 - ssidLen - offset);
      if      (cmdKey == SetStaSsidPass)      Wifi::instance()->setStaConfig(ssid, pass, true);
      else if (cmdKey == SetApSsidPass)       Wifi::instance()->setApConfig(ssid, pass, true);
      else if (cmdKey == AppendAltApSsidPass) Wifi::instance()->appendAltApConnectionSsidPassword(ssid, pass);
      Wifi::instance()->saveConfig();
      // if (args[0]) {  // sih sil
      //   uint32_t sih, sil;
      //   memcpy(&sih, args + 1, 4);
      //   memcpy(&sil, args + 5, 4);
      //   System::instance()->setConnectionSi(sih, sil);
      // }
      break;
    }

    case ClearAltApSsidPassList:
      Wifi::instance()->clearAltApConnectionSsidPassword();
      Wifi::instance()->saveConfig();
      break;

    case GetSystemDeployMode: {
      const char * modeStr = deployModeStr(System::instance()->deployMode());
      if (retFmt == JSON) {
        replyJsonResult(_delegate, modeStr, cmdKey, userdata);
      }
      else {
        _delegate->replyMessage(modeStr, strlen(modeStr), userdata);
      }
      break;
    }

    case SetSystemDeployMode:
      System::instance()->setDeployMode((DeployMode)args[0]);
      break;

    case SetSensorType:
      System::instance()->setSensorType((SensorType)args[0], (SensorType)args[1]);
      break;

    case SetAlertEnableConfig:
      System::instance()->setAlertPnEnabled(args[0] == 1);
      System::instance()->setAlertSoundEnabled(args[1] == 1);
      break;

    case SetSensorAlertConfig: {
      FloatBytes fbl, fbg;
      size_t bLen = FloatLen * 2 + 2;
      System *sys = System::instance();
      for (int i=0; i<SensorDataTypeCount; ++i) {
        SensorDataType sdt = (SensorDataType)i;
        memcpy(fbl.bytes, args+i*bLen+2,          FloatLen);
        memcpy(fbg.bytes, args+i*bLen+2+FloatLen, FloatLen);
        sys->setAlert(sdt, args[i*bLen+0]==1, args[i*bLen+1]==1, fbl.v, fbg.v);
      }
      sys->resetAlertReactiveCounter();
      break;
    }

    case GetAlertConfig:
      if (retFmt == JSON) {
        System *sys = System::instance();
        size_t packCount = 0;
        sprintf(_strBuf + packCount, "{\"cmd\":\"%s\",\"ret\":{\"enpn\":%s,\"ensnd\":%s,\"vals\":{",
                cmdKeyToStr(cmdKey),
                sys->alertPnEnabled() ? "true" : "false",
                sys->alertSoundEnabled() ? "true" : "false");
        packCount += strlen(_strBuf + packCount);

        Alerts *alerts = sys->alerts();
        for (int i=0; i<SensorDataTypeCount; ++i) {
          sprintf(_strBuf + packCount, "\"%s\":{\"len\":%s,\"gen\":%s,\"lval\":%.2f,\"gval\":%.2f}%s",
                  sensorDataTypeStr((SensorDataType)i),
                  alerts->sensors[i].lEnabled ? "true" : "false",
                  alerts->sensors[i].gEnabled ? "true" : "false",
                  alerts->sensors[i].lValue,
                  alerts->sensors[i].gValue,
                  i < SensorDataTypeCount - 1 ? "," : "");
          packCount += strlen(_strBuf + packCount);
        }
        sprintf(_strBuf + packCount, "}}}");
        packCount += strlen(_strBuf + packCount);
        _delegate->replyMessage(_strBuf, packCount, userdata);
      }
      break;

    case CheckPNTokenEnabled:
      if (retFmt == JSON) {
        sprintf(_strBuf, "{\"ret\":{\"en\":%s}, \"cmd\":\"%s\"}",
                System::instance()->tokenEnabled((MobileOS)args[0], (const char*)(args+1)) ? "true" : "false",
                cmdKeyToStr(cmdKey));
        _delegate->replyMessage(_strBuf, strlen(_strBuf), userdata);
      }
      break;

    case SetPNToken:
      System::instance()->setPnToken(args[0]==1, (MobileOS)args[1], (const char*)(args+2));
      break;

    case SetDebugFlag:
      System::instance()->setDebugFlag(args[0]);
      break;

    default:
      break;
  }

  return 0;
}
