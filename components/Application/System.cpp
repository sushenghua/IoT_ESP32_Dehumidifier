/*
 * System.h system function
 * Copyright (c) 2017 Shenghua Su
 *
 */

/////////////////////////////////////////////////////////////////////////////////////////
// tasks to launch
/////////////////////////////////////////////////////////////////////////////////////////
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Config.h"
#include "AppLog.h"
#include "System.h"

//----------------------------------------------
// peripheral task loop control
//----------------------------------------------
bool     _enablePeripheralTaskLoop = true;
enum TaskState {
  TaskEmpty       = 0,
  TaskRunning     = 1,
  TaskPaused      = 2,
  TaskNoResponse  = 3,
  TaskHandleErr   = 4,
  TaskKilled      = 5,
  TaskDebug       = 6
};

// watch dog
// #include "esp_task_wdt.h"
// esp_task_wdt_init(); // put it before task loop
// esp_task_wdt_feed(); // put it in task loop

//----------------------------------------------
// Wifi task
//----------------------------------------------
#include "Wifi.h"
#include "esp_netif.h"

TaskHandle_t wifiTaskHandle;
void wifi_task(void *pvParameters)
{
  // config and start wifi
  if (Wifi::instance()->loadConfig()) {
    APP_LOGI("[Wifi]", "load config succeeded");
  }
  else {
    Wifi::instance()->setDefaultConfig();
    Wifi::instance()->setStaConfig("ssid", "ssidpasswd");
    if (Wifi::instance()->saveConfig()) {
      APP_LOGI("[Wifi]", "save config succeeded");
    }
  }
  // set wifi mode accordingly
  DeployMode deployMode = System::instance()->deployMode();
  // if (deployMode == MQTTClientMode) Wifi::instance()->setWifiMode(WIFI_MODE_STA);
  // else if (deployMode == HTTPServerMode) Wifi::instance()->setWifiMode(WIFI_MODE_AP);
  // else if (deployMode == MQTTClientAndHTTPServerMode) Wifi::instance()->setWifiMode(WIFI_MODE_APSTA);
  Wifi::instance()->setWifiMode(WIFI_MODE_STA);

  Wifi::instance()->init();
  while (true) {
    if (System::instance()->wifiOn()) {
      Wifi::instance()->start();
    }
    else {
      Wifi::instance()->stop();
    }
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}


//----------------------------------------------
// SNTP time task
//----------------------------------------------
#include "SNTP.h"

TaskHandle_t sntpTaskHandle;
static void sntp_task(void *pvParams)
{
  SNTP::init();
  SNTP::waitSync();

  SNTP::test();
  vTaskDelete(sntpTaskHandle);
}


//----------------------------------------------
// deep sleep
//----------------------------------------------
#include "esp_sleep.h"
esp_sleep_wakeup_cause_t _wakeupCause = ESP_SLEEP_WAKEUP_UNDEFINED;
esp_sleep_wakeup_cause_t getWakeupCause()
{
  if (_wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED)
    _wakeupCause = esp_sleep_get_wakeup_cause();
  return _wakeupCause;
}

bool isDeepSleepReset()
{
  return getWakeupCause() == ESP_SLEEP_WAKEUP_TIMER;
}


// //----------------------------------------------
// // display tasks
// //----------------------------------------------
// // #include "ST7789V.h"
// // ST7789V dev;// static ILI9341 dev;
// #include "ILI9341.h"
// #include "SensorDisplayController.h"
// #define DISPLAY_TASK_DELAY_UNIT  100
// ILI9341 dev;
// static SensorDisplayController dc(&dev);
// TaskHandle_t displayTaskHandle = 0;
// TaskState _displayTaskState = TaskEmpty;
// bool _hasScreenMessage = false;

// uint16_t _displayDaemonInactiveTicks = 0;

// void display_task(void *p)
// {
//   if (isDeepSleepReset()) {
//     APP_LOGC("[display_task]", "deep sleep timer wakeup reset");
//   }

//   dc.init();
//   APP_LOGC("[display_task]", "start running ...");
//   // _displayDaemonInactiveTicks = 0;
//   while (true) {
//     if (_enablePeripheralTaskLoop) {
//       dc.update();
//       _displayDaemonInactiveTicks = 1; _displayTaskState = TaskRunning;
//     }
//     else if (_hasScreenMessage) {
//       dc.update(); _hasScreenMessage = false;
//       _displayDaemonInactiveTicks = 1; _displayTaskState = TaskRunning;
//     }
//     else _displayTaskState = TaskPaused;
//     vTaskDelay(DISPLAY_TASK_DELAY_UNIT / portTICK_RATE_MS);
//   }
// }

// void _resetDisplay()
// {
//   _displayDaemonInactiveTicks = 0;

//   dc.setUpdateDisabled(true);
//   dc.reset();
//   dc.setUpdateDisabled(false);
//   APP_LOGW("[display_guard_task]", "reset done");
//   // System::instance()->setRestartRequest();
// }

// #ifdef DEBUG_FLAG_ENABLED
// void _debugDisplay()
// {
//   _displayTaskState = TaskDebug;
//   dev.spi_bug();
//   APP_LOGC("[display_guard_task]", "debug call done");
// }
// #endif

// //----------------------------------------------
// // display guard task
// //----------------------------------------------
// #define DEBUG_FLAG_NULL                         0
// #define DEBUG_FLAG_RESET_DISPLAY                1
// #define DEBUG_FLAG_TRY_TRIGGER_DISPLAY_BUG      2
// #define DEBUG_FLAG_ASSERTION_FAIL               3
// uint8_t _debugFlag = DEBUG_FLAG_NULL;

// #ifdef DEBUG_PN
// void _genDebugMsgPN(const char* tag, const char* msg);
// #endif

// uint16_t _displayErrHandleWaitTicks = 0;
// #define RESTART_DISPLAY_ERR_HANDLE_WAIT_TICKS   10

// #define DISPLAY_GUARD_TASK_DELAY_UNIT           500
// TaskHandle_t displayGuardTaskHandle = 0;

// static void display_guard_task(void *pvParams = NULL)
// {
// #ifdef DEBUG_PN
//   _genDebugMsgPN("d", "display guard task enabled");
// #endif
//   while (true) {
//     if (_displayTaskState == TaskNoResponse) {
//       _displayTaskState = TaskHandleErr;
//       // APP_LOGW("[daemon_task]", "--->relaunch display task, free RAM: %d bytes", esp_get_free_heap_size());
//       APP_LOGE("[display_guard_task]", "display task no response -> reset");
// #ifdef DEBUG_PN
//       _genDebugMsgPN("d", "display task inactive");
// #endif
//       _resetDisplay();
//       _displayErrHandleWaitTicks = 0;
//     }
//     else if (_displayTaskState == TaskHandleErr) {
//       if (_displayErrHandleWaitTicks < RESTART_DISPLAY_ERR_HANDLE_WAIT_TICKS) {
//         APP_LOGE("[display_guard_task]", "error handling ... %d", _displayErrHandleWaitTicks);
//       }
//       else {
//         APP_LOGE("[display_guard_task]", "error handling ... restart");
//         System::instance()->deepSleepReset();
//       }
//       ++_displayErrHandleWaitTicks;
//     }
// #ifdef DEBUG_FLAG_ENABLED
//     if (_debugFlag != DEBUG_FLAG_NULL) {
//       APP_LOGC("[display_guard_task]", "debug flag set to %d", _debugFlag);
//       if (_debugFlag == DEBUG_FLAG_TRY_TRIGGER_DISPLAY_BUG) _debugDisplay();
//       else if (_debugFlag == DEBUG_FLAG_RESET_DISPLAY) _resetDisplay();
//       else if (_debugFlag == DEBUG_FLAG_ASSERTION_FAIL) assert(false);
//       _debugFlag = DEBUG_FLAG_NULL;
//     }
// #endif
//     vTaskDelay(DISPLAY_GUARD_TASK_DELAY_UNIT / portTICK_PERIOD_MS);
//   }
// }


//----------------------------------------------
// status check tasks
//----------------------------------------------
// #include "Adc.h"
// #include "InputMonitor.h"
// #include "PowerManager.h"

// #define BATTERY_LEVEL_TO_PWR_OFF  15
// #define SYS_EVENT_POWER_LOW       101  // value different from INPUT_EVENT_xxx in InputMonitor
// #define STATUS_TASK_DELAY_UNIT  100

// #define TIME_WIFI_UPDATE_COUNT  5
// uint8_t _timeWifiUpdateCount = 0;

// // bool _websocketConnected = false;

// PowerManager powerManager;
// bool _hasPwrEvent = true;
// PowerManager::ChargeStatus _chargeStatus = PowerManager::NotCharging;

TaskState _statusTaskState = TaskEmpty;
// void status_check_task(void *p)
// {
//   InputMonitor::instance()->init();   // this will launch another task
//   powerManager.init();
//   _statusTaskState = TaskRunning;
//   while (true) {
//     if (_enablePeripheralTaskLoop) {
//       // update time and wifi status every 0.5 second
//       ++_timeWifiUpdateCount;
//       if (_timeWifiUpdateCount >= TIME_WIFI_UPDATE_COUNT) {
//         // wifi status
//         if (!Wifi::instance()->started())
//           dc.setNetworkState(NetworkOff);
//         else if (System::instance()->deployMode() == HTTPServerMode)
//           dc.setNetworkState(Wifi::instance()->apStaConnected()? NetworkConnected : NetworkNotConnected);
//         else
//           dc.setNetworkState(Wifi::instance()->connected()? NetworkConnected : NetworkNotConnected);

//         // time
//         dc.setTimeUpdate(true);

//         _timeWifiUpdateCount = 0;
//       }
//       // battery level check
//       if (powerManager.batteryLevelPollTick()) {
//         if (powerManager.batteryLevel() < BATTERY_LEVEL_TO_PWR_OFF) {
//           System::instance()->onEvent(SYS_EVENT_POWER_LOW);
//         }
//         dc.setBatteryLevel(powerManager.batteryLevel());
//       }
//       // battery charge check
//       if (_hasPwrEvent) {
//         _chargeStatus = powerManager.chargeStatus();
// #ifdef DEBUG_BATTERY_LIFE
//         if (_chargeStatus == PowerManager::NotCharging && powerManager.batteryLevel() > 95)
//           System::instance()->clearMaintenance();
// #endif
//         dc.setBatteryCharge(_chargeStatus == PowerManager::PreCharge || _chargeStatus == PowerManager::FastCharge); 
//         _hasPwrEvent = false;
//       }
//     }
//     else {
//       _statusTaskState = TaskPaused;
//     }
//     vTaskDelay(STATUS_TASK_DELAY_UNIT / portTICK_RATE_MS);
//   }
// }


//----------------------------------------------
// touch check task
//----------------------------------------------
// #include "TouchPad.h"

// TaskHandle_t touchPadTaskHandle;
// static void touch_pad_task(void *pvParams)
// {
//   TouchPad tp(TOUCH_PAD_NUM5);
//   tp.init();

//   while (true) {
//     tp.readValue();
//     vTaskDelay(200 / portTICK_PERIOD_MS);
//   }
// }


//----------------------------------------------
// sensor tasks
//----------------------------------------------
#include "SHT3xSensor.h"
// #include "PMSensor.h"
// #include "CO2Sensor.h"
// #include "OrientationSensor.h"
// #include "TSL2561.h"
#include "SensorDataPacker.h"

// uint32_t _lAlertMask = 0;
// uint32_t _gAlertMask = 0;
// float    _alertValue[SensorDataTypeCount];

// void checkAlert(SensorDataType type, float value)
// {
//   System *sys = System::instance();
//   _alertValue[type] = value;
//   uint32_t mask = (uint32_t)sensorAlertMask(type);
//   TriggerAlert trigger = sys->sensorValueTriggerAlert(type, value);
//   if (trigger == TriggerL) { _lAlertMask |= mask; _gAlertMask &= (~mask); }
//   else if (trigger == TriggerG) { _lAlertMask &= (~mask); _gAlertMask |= mask; }
//   else { _lAlertMask &= (~mask); _gAlertMask &= (~mask); }

//   if (sys->alertSoundEnabled()) {
//     if (_lAlertMask || _gAlertMask) sys->turnAlertSoundOn(true);
//     else sys->turnAlertSoundOn(false);
//   }
// }

SHT3xSensor sht3xSensor0;
SHT3xSensor sht3xSensor1(1);
TaskHandle_t sht3xSensorTaskHandle;
TaskState _sht3xSensorTaskState = TaskEmpty;
void sht3x_sensor_task(void *p)
{
  sht3xSensor0.init();
  sht3xSensor1.init();
  // sht3xSensor0.setDisplayDelegate(&dc);
  SensorDataPacker::sharedInstance()->init();
  SensorDataPacker::sharedInstance()->setTempHumidSensor(&sht3xSensor0, &sht3xSensor1);
  _sht3xSensorTaskState = TaskRunning;
  while (true) {
    if (_enablePeripheralTaskLoop) {
      sht3xSensor0.sampleData();
      sht3xSensor1.sampleData();
      // checkAlert(TEMP, sht3xSensor0.tempHumidData().temp);
      // checkAlert(HUMID, sht3xSensor0.tempHumidData().humid);
    }
    else _sht3xSensorTaskState = TaskPaused;
    vTaskDelay(500/portTICK_PERIOD_MS);
  }
}

// TaskHandle_t pmSensorTaskHandle;
// TaskState _pmSensorTaskState = TaskEmpty;
// void pm_sensor_task(void *p)
// {
//   PMSensor pmSensor;
//   pmSensor.init();
//   pmSensor.setDisplayDelegate(&dc);
//   SensorDataPacker::sharedInstance()->init();
//   SensorDataPacker::sharedInstance()->setPmSensor(&pmSensor);
//   _pmSensorTaskState = TaskRunning;
//   while (true) {
//     if (_enablePeripheralTaskLoop) {
//       pmSensor.sampleData(3000);
//       checkAlert(PM, pmSensor.pmData().aqiPm());
//       if (System::instance()->devCapability() & HCHO_CAPABILITY_MASK)
//         checkAlert(HCHO, pmSensor.hchoData().hcho);
//     }
//     else _pmSensorTaskState = TaskPaused;
//     vTaskDelay(500/portTICK_RATE_MS);
//   }
// }

// TaskHandle_t co2SensorTaskHandle = NULL;
// TaskState _co2SensorTaskState = TaskEmpty;
// void co2_sensor_task(void *p)
// {
//   CO2Sensor co2Sensor;
//   co2Sensor.init();
//   co2Sensor.setDisplayDelegate(&dc);
//   SensorDataPacker::sharedInstance()->init();
//   SensorDataPacker::sharedInstance()->setCO2Sensor(&co2Sensor);
//   _co2SensorTaskState = TaskRunning;
//   vTaskDelay(3000/portTICK_RATE_MS); // delay 3 seconds
//   while (true) {
//     if (_enablePeripheralTaskLoop) {
//       co2Sensor.sampleData(3000);
//       checkAlert(CO2, co2Sensor.co2Data().co2);
//     }
//     else _co2SensorTaskState = TaskPaused;
//     vTaskDelay(1000/portTICK_RATE_MS);
//   }
// }

// TaskHandle_t tsl2561SensorTaskHandle;
// TaskState   _tsl2561SensorTaskState = TaskEmpty;
// uint32_t    _luminsity = 0;
// void tsl2561_sensor_task(void *p)
// {
//   TSL2561 tsl2561Sensor;
//   tsl2561Sensor.init();
//   tsl2561Sensor.setDisplayDelegate(&dc);
//   SensorDataPacker::sharedInstance()->init();
//   SensorDataPacker::sharedInstance()->setLmSensor(&tsl2561Sensor);
//   _tsl2561SensorTaskState = TaskRunning;
//   while (true) {
//     if (_enablePeripheralTaskLoop) {
//       tsl2561Sensor.sampleData();
//       if (System::instance()->displayAutoAdjustOn()) {
//         _luminsity = tsl2561Sensor.luminosityData().luminosity;
//         // APP_LOGC("[TSL2561 Task]", "lux: %d", _luminsity);
//         if (_luminsity >= 100) dc.fadeBrightness(100);
//         else if (_luminsity < 10) dc.fadeBrightness(10);
//         else dc.fadeBrightness(_luminsity);
//       }
//       else if (_luminsity != 100) {
//         _luminsity = 100;
//         dc.fadeBrightness(100);
//       }
//     }
//     else _tsl2561SensorTaskState = TaskPaused;
//     vTaskDelay(1000/portTICK_RATE_MS);
//   }
// }

// #define ORI_SENSOR_TEMP_READ_COUNT 30
// TaskHandle_t orientationSensorTaskHandle;
// uint16_t _oriSensorTempReadCount = 0;
// float _oriSensorTemperature = 0;
// TaskState _orientationSensorTaskState = TaskEmpty;
// void orientation_sensor_task(void *p)
// {
//   OrientationSensor orientationSensor;
//   orientationSensor.init();
//   orientationSensor.setDisplayDelegate(&dc);
//   _orientationSensorTaskState = TaskRunning;
//   while (true) {
//     if (_enablePeripheralTaskLoop) {
//       orientationSensor.sampleData();
//       if (_oriSensorTempReadCount++ == ORI_SENSOR_TEMP_READ_COUNT) {
//         _oriSensorTemperature = orientationSensor.readTemperature();
//         sht3xSensor.setMainboardTemperature(_oriSensorTemperature,
//           _chargeStatus == PowerManager::PreCharge || _chargeStatus == PowerManager::FastCharge);
//         _oriSensorTempReadCount = 0;
//       }
//     }
//     else _orientationSensorTaskState = TaskPaused;
//     vTaskDelay(100/portTICK_RATE_MS);
//   }
// }


//----------------------------------------------
// Mqtt client task
//----------------------------------------------
#ifdef ENABLE_MQTT_TASK
// the following line must be place after #include "ILI9341.h", 
// as mongoose.h has macro write (s, b, l)
#include "MqttClient.h"
#include "CmdEngine.h"
#include "SharedBuffer.h"

MqttClient mqtt;

// // ------ generate alert push notification request string
char *_alertStringBuf = NULL;

// size_t genAlertPushNotificationJsonString(uint32_t mask, const char* tag)
// {
//   MobileTokens *tokens = System::instance()->mobileTokens();

//   // return 0 on no alert or no token
//   if (mask == 0 || tokens->count == 0) return 0;

//   size_t packCount = 0;
//   // print: tag, tokens key, tokens content list starting '['
//   sprintf(_alertStringBuf + packCount, "{\"tag\":\"%s\",\"tokens\":[", tag);
//   packCount += strlen(_alertStringBuf + packCount);

//   // print: token list content
//   size_t onTokenCount = 0;
//   for (uint8_t i=0; i<tokens->count; ++i) {
//     MobileToken &token = tokens->token(i);
//     if (token.on) {
//       sprintf(_alertStringBuf + packCount, "%s{\"token\":\"%s\",\"os\":\"%s\",\"grp\":\"%s\"}",
//               onTokenCount > 0 ? "," : "", token.str, mobileOSStr(token.os), token.groupLen > 0 ? token.group : "");
//       packCount += strlen(_alertStringBuf + packCount);
//       ++onTokenCount;
//     }
//   }

//   // no alert-active token
//   if (onTokenCount == 0) return 0;

//   // print: tokens content list ending ']', val key, val content starting '{'
//   sprintf(_alertStringBuf + packCount, "],\"val\":{");
//   packCount += strlen(_alertStringBuf + packCount);

//   // print: val content
//   uint8_t sensorItemCount = 0;
//   for (uint8_t t=PM; t<SensorDataTypeCount; ++t) {
//     if (mask & sensorAlertMask((SensorDataType)t)) {
//       sprintf(_alertStringBuf + packCount, sensorDataValueStrFormat((SensorDataType)t),
//               sensorItemCount == 0 ? "" : ",", _alertValue[t]);
//       packCount += strlen(_alertStringBuf + packCount);
//       ++sensorItemCount;
//     }
//   }

//   // print: val content ending '}', dev name, whole json ending '}'
//   sprintf(_alertStringBuf + packCount, "},\"dev\":\"%s\"}", System::instance()->deviceName());
//   packCount += strlen(_alertStringBuf + packCount);

//   return packCount;
// }

// ------ use counter to check and publish alert push notification request
#define REACTIVE_COUNT_FOR_BOOT               1000 // 1000 * 10ms = 10 seconds
#define REACTIVE_COUNT_FOR_NO_RECENT_PUB      6000 // 1000 * 10ms = 60 seconds
uint32_t _alertReactiveCount;
uint32_t _lAlertReactiveCounter;
uint32_t _gAlertReactiveCounter;

void _resetAlertReactiveCounter(bool deepSleepReset = false)
{
  _alertReactiveCount = System::instance()->alerts()->reactiveTimeCount;

  if (deepSleepReset) {
    SysResetRestore *restoreData = System::instance()->resetRestoreData();
    _lAlertReactiveCounter = restoreData->lAlertReactiveCounter;
    _gAlertReactiveCounter = restoreData->gAlertReactiveCounter;
  }
  else {
    _lAlertReactiveCounter = _alertReactiveCount - REACTIVE_COUNT_FOR_BOOT;
    _gAlertReactiveCounter = _alertReactiveCount - REACTIVE_COUNT_FOR_BOOT;
  }
}

#define NPS_TOPIC           "api/nps"

void _sendLAlertPushNotification()
{
//   // assume no recent publish; if recent publish occurs, wait longer for next check
//   _lAlertReactiveCounter = _alertReactiveCount - REACTIVE_COUNT_FOR_NO_RECENT_PUB;
//   if (System::instance()->alertPnEnabled()) {
//     size_t jsonSize = genAlertPushNotificationJsonString(_lAlertMask, "l");
//     if (jsonSize > 0) {
// #ifdef LOG_ALERT
//       APP_LOGC("[mqtt_task]", "push L alert PN request, free RAM: %d bytes", esp_get_free_heap_size());
// #endif
//       mqtt.publish(NPS_TOPIC, _alertStringBuf, jsonSize, 0);
//       _lAlertReactiveCounter = 0;
//     }
//   }
}

void _sendGAlertPushNotification()
{
//   // assume no recent publish; if recent publish occurs, wait longer for next check
//   _gAlertReactiveCounter = _alertReactiveCount - REACTIVE_COUNT_FOR_NO_RECENT_PUB;
//   if (System::instance()->alertPnEnabled()) {
//     size_t jsonSize = genAlertPushNotificationJsonString(_gAlertMask, "g");
//     if (jsonSize > 0) {
// #ifdef LOG_ALERT
//       APP_LOGC("[mqtt_task]", "push G alert PN request, free RAM: %d bytes", esp_get_free_heap_size());
// #endif
//       mqtt.publish(NPS_TOPIC, _alertStringBuf, jsonSize, 0);
//       _gAlertReactiveCounter = 0;
//     }
//   }
}

// // ------ debug message push notification
// #ifdef DEBUG_PN

// bool _hasDebugMsg = false;
// char _debugMsg[256];

// void _genDebugMsgPN(const char* tag, const char* msg)
// {
//   MobileTokens *tokens = System::instance()->mobileTokens();
//   if (tokens->count == 0) return;

//   size_t packCount = 0;

//   sprintf(_debugMsg + packCount, "{\"tag\":\"%s\",\"tokens\":[", tag);
//   packCount += strlen(_debugMsg + packCount);

//   size_t iosTokenCount = 0;
//   for (uint8_t i=0; i<tokens->count; ++i) {
//     MobileToken &token = tokens->token(i);
//     if (token.os == iOS) {
//       sprintf(_debugMsg + packCount, "%s{\"token\":\"%s\",\"os\":\"%s\",\"grp\":\"%s\"}",
//               iosTokenCount > 0 ? "," : "", token.str, mobileOSStr(token.os), token.groupLen > 0 ? token.group : "");
//       packCount += strlen(_debugMsg + packCount);
//       ++iosTokenCount;
//       break;
//     }
//   }
//   if (iosTokenCount == 0) return;

//   sprintf(_debugMsg + packCount, "],\"msg\":\"%s\",\"dev\":\"%s\"}", msg, System::instance()->deviceName());
//   packCount += strlen(_debugMsg + packCount);

//   _hasDebugMsg = true;
// }

// void _sendDebugMsgPN()
// {
//   if (mqtt.connected() && _hasDebugMsg) {
//     mqtt.publish(NPS_TOPIC, _debugMsg, strlen(_debugMsg), 0);
//     _hasDebugMsg = false;
//   }
// }

// #endif
#endif // #ifdef ENABLE_MQTT_TASK

// ------ mqtt task
#ifdef ENABLE_MQTT_TASK
#include "Crypto.h"
#include "certs/mqtt_user.h"
#include "certs/mqtt_passwd.h"
static void mqtt_task(void *pvParams)
{
  CmdEngine cmdEngine;
  {
    char user[32];
    char pass[32];
    unsigned char v1[] = "CkECr+xGO4YKbicw";
    unsigned char v2[] = "cBh++uagobC8s1bK";
    size_t len;
    decryptBase64(mqttUser,   strlen(mqttUser),   '\0', v1, v2, user, &len);
    decryptBase64(mqttPasswd, strlen(mqttPasswd), '\0', v1, v2, pass, &len);
// ESP_LOG_BUFFER_HEXDUMP("user", user, 64, ESP_LOG_INFO);
// ESP_LOG_BUFFER_HEXDUMP("pass", pass, 64, ESP_LOG_INFO);
// APP_LOGC("[mqtt_task]", "user: %s, pass: %s", user, pass);
    mqtt.setUserPassword(user, pass);
    // mqtt.setUserPassword("aqmonitor", "upuPDOK6+zsOwRBKYG9Am");
  }
  mqtt.init();
  mqtt.start();

  cmdEngine.setProtocolDelegate(&mqtt);
  cmdEngine.init();
  cmdEngine.enableUpdate(&mqtt);

  _resetAlertReactiveCounter(isDeepSleepReset());
  _alertStringBuf = SharedBuffer::msgBuffer();

  while (true) {
    // mqtt.poll();
#ifdef DEBUG_PN
    _sendDebugMsgPN();
#endif
    if (++_lAlertReactiveCounter == _alertReactiveCount) _sendLAlertPushNotification();
    if (++_gAlertReactiveCounter == _alertReactiveCount) _sendGAlertPushNotification();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
#endif // ENABLE_MQTT_TASK


//----------------------------------------------
// Http server task
//----------------------------------------------
// the following line must be place after #include "ILI9341.h", 
// as mongoose.h has macro write (s, b, l)
#ifndef ENABLE_MQTT_TASK
#include "CmdEngine.h"
#include "WebsocketClient.h"
#endif
#include "HttpServer.h"

WebsocketClient ws;
HttpServer server;

static void http_task(void *pvParams)
{
  CmdEngine cmdEngine;

  server.start(false);

  cmdEngine.setProtocolDelegate(&server);
  cmdEngine.init();

  ws.init();
  cmdEngine.enableUpdate(&ws, &server);

  while (true) {
    if (ws.reinitRequried()) {
      ws.deinit();
      vTaskDelay(10 / portTICK_PERIOD_MS);
      ws.init();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}


//----------------------------------------------
// buzzer task
//----------------------------------------------
// #include "Buzzer.h"
// Buzzer buzzer;
// bool _buzzerTriggered = false;
// uint16_t _buzzerSoundTicksCounter = 0;
// #define BUZZER_SOUND_ON1_TICKS    0
// #define BUZZER_SOUND_OFF1_TICKS   1
// #define BUZZER_SOUND_ON2_TICKS    2
// #define BUZZER_SOUND_OFF2_TICKS   3
// #define BUZZER_SOUND_CYCLE_TICKS 10

// TaskHandle_t buzzerTaskHandle;
// static void buzzer_task(void *pvParams = NULL)
// {
//   buzzer.init();
//   while (true) {
//     if (_buzzerTriggered) {
//       if (_buzzerSoundTicksCounter == BUZZER_SOUND_ON1_TICKS) buzzer.start();
//       else if (_buzzerSoundTicksCounter == BUZZER_SOUND_OFF1_TICKS) buzzer.stop();
//       else if (_buzzerSoundTicksCounter == BUZZER_SOUND_ON2_TICKS) buzzer.start();
//       else if (_buzzerSoundTicksCounter == BUZZER_SOUND_OFF2_TICKS) buzzer.stop();
//       ++_buzzerSoundTicksCounter;
//       if (_buzzerSoundTicksCounter == BUZZER_SOUND_CYCLE_TICKS) _buzzerSoundTicksCounter = 0;
//     }
//     vTaskDelay(100 / portTICK_PERIOD_MS);
//   }
// }


//----------------------------------------------
// fan control task
//----------------------------------------------
#include "DehumidFan.h"

TaskHandle_t humidControlTaskHandle;
// TaskState _humidControlTaskState = TaskEmpty;

// --- mode management
int     _currentMode = FAN_MODE_UNDEFINED;

float   _lHumid  = 999.f;
float   _gHumid  = 999.f;

#define HUMID_CONTROL_TASK_DELAY_UNIT         1000
float   _fan_cd  = 60.f;
uint    _second_elapsed = 0;

DehumidFan dehumidFan;

static void humid_control_task(void *pvParams = NULL)
{
  dehumidFan.init();
  SensorDataPacker::sharedInstance()->setDehumidFan(&dehumidFan);

  FanMode *fm = System::instance()->fanMode();
  // _humidControlTaskState = TaskRunning;
  while (true) {
    if (_currentMode != fm->mode) {
      if (fm->mode == FAN_MODE_AUTO) {
        _lHumid = fm->lthr;
        _gHumid = fm->gthr;
      }
      else if (fm->mode == FAN_MODE_DELAY) {
        _fan_cd = fm->min;
        _second_elapsed = 0;
      }
      else if (fm->mode == FAN_MODE_ON) {
        dehumidFan.setFan1On(true);
        dehumidFan.setFan2On(true);  
      }
      else if (fm->mode == FAN_MODE_OFF) {
        dehumidFan.setFan1On(false);
        dehumidFan.setFan2On(false);  
      }
      _currentMode = fm->mode;
      APP_LOGC("[System]", "update mode: %d", _currentMode);
    }
    else {
      if (fm->mode == FAN_MODE_AUTO) {
        float h0 = sht3xSensor0.tempHumidData().humid;
        float h1 = sht3xSensor1.tempHumidData().humid;
        if (h0 > _gHumid) dehumidFan.setFan1On(true);
        else if (h0 < _lHumid) dehumidFan.setFan1On(false);
        if (h1 > _gHumid) dehumidFan.setFan2On(true);
        else if (h1 < _lHumid) dehumidFan.setFan2On(false);

        // APP_LOGC("[System]", "auto mode, h0: %.1f, h1: %.1f, l: %.1f, g: %.1f", h0, h1, _lHumid, _gHumid);
      }
      else if (fm->mode == FAN_MODE_DELAY) {
        dehumidFan.setFan1On(_fan_cd > 0.1f);
        dehumidFan.setFan2On(_fan_cd > 0.1f);
      }
      // else {
      //   dehumidFan.setFan1On(true);
      //   dehumidFan.setFan2On(true);
      // }
    }
    vTaskDelay(HUMID_CONTROL_TASK_DELAY_UNIT / portTICK_PERIOD_MS);
    if (++_second_elapsed == 60) {
      _second_elapsed = 0;
      _fan_cd -= 1.f;
      if (_fan_cd < 0.1f) _fan_cd = 0.f;
    }
  }
}


//----------------------------------------------
// daemon task
//----------------------------------------------
#define DAEMON_TASK_DELAY_UNIT                  50
// #define DISPLAY_TASK_ALLOWED_INACTIVE_MAX_TICKS 50   // time(ms): this ticks * DAEMON_TASK_DELAY_UNIT
bool _hasRebootRequest = false;
bool _hasWebsocketClientStopRequest = false;
bool _hasWebsocketClientReinitRequest = false;
bool _hasHttpServerStopRequest = false;

static void daemon_task(void *pvParams = NULL)
{
  while (true) {
    if (_hasRebootRequest) {
      if (ws.connected()) {}
      else if (server.websocketConnectionCount() > 0) {}
      else {
        System::instance()->restart();
      }
    }
    if (_hasHttpServerStopRequest) {
      server.stop();
      _hasHttpServerStopRequest = false;
    }
    if (_hasWebsocketClientStopRequest) {
      ws.stop();  // ws.close();
      _hasWebsocketClientStopRequest = false;
    }
    if (_hasWebsocketClientReinitRequest) {
      ws.deinit();
       vTaskDelay(DAEMON_TASK_DELAY_UNIT / portTICK_PERIOD_MS);
      ws.init();
      _hasWebsocketClientReinitRequest = false;
    }

    vTaskDelay(DAEMON_TASK_DELAY_UNIT / portTICK_PERIOD_MS);
  }
}


//**********************************************
// prepare env
//**********************************************
#include "Semaphore.h"
#include "I2cPeripherals.h"

static void beforeCreateTasks()
{
  // _dcUpdateSemaphore = xSemaphoreCreateMutex();
  Semaphore::init();
  I2cPeripherals::init();
}


/////////////////////////////////////////////////////////////////////////////////////////
// enum types
/////////////////////////////////////////////////////////////////////////////////////////
static const char * const DeployModeStr[] = {
  "HTTPServerMode",                  // 0
  "MQTTClientMode",                  // 1
  "MQTTClientAndHTTPServerMode"      // 2
};

const char * deployModeStr(DeployMode mode)
{
  return DeployModeStr[mode];
}

static const char * const MobileOSStr[] = {
  "ios",        // 0
  "android"     // 1
};

const char* mobileOSStr(MobileOS os)
{
  return MobileOSStr[os];
}

/////////////////////////////////////////////////////////////////////////////////////////
// Sytem class
/////////////////////////////////////////////////////////////////////////////////////////
#include "NvsFlash.h"
#include "esp_system.h"
#include "esp_flash_encrypt.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_mac.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include <string.h>
#include "ProductConfig.h"
#include "AppUpdaterConfig.h"

static char MAC_ADDR[13] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF };

static void _initMacADDR()
{
  uint8_t macAddr[6];
  char *target = MAC_ADDR;
  esp_efuse_mac_get_default(macAddr);
  for (int i = 0; i < 6; ++i) {
    sprintf(target, "%02x", macAddr[i]);
    target += 2;
  }
  MAC_ADDR[12] = '\0';
}

static System _sysInstance;

System * System::instance()
{
  return &_sysInstance;
}

System::System()
: _state(Uninitialized)
, _dataNeedToSave(false)
, _currentSessionLife(0)
{
  _setDefaultConfig();
  _data.init();
}

void System::_setDefaultConfig()
{
  _data.config1.wifiOn = true;
  _data.config1.displayAutoAdjustOn = false;
  _data.config1.deployMode = HTTPServerMode;
  // _data.config1.deployMode = MQTTClientMode;
  // _data.config2.pmSensorType =  PRODUCT_PM_SENSOR;
  // _data.config2.co2SensorType = PRODUCT_CO2_SENSOR;
  // _data.config2.devCapability = ( capabilityForSensorType(_data.config2.pmSensorType) |
  //                                 capabilityForSensorType(_data.config2.co2SensorType) );
  // _data.config2.devCapability |= DEV_BUILD_IN_CAPABILITY_MASK;
  // _data.config2.devCapability |= ORIENTATION_CAPABILITY_MASK;
  // _data.config2.devCapability |= LUMINOSITY_CAPABILITY_MASK;
  _data.config2.devName[DEV_NAME_MAX_LEN] = '\0'; // null terminated
  sprintf(_data.config2.devName, "%s_%.*s", "DishwasherFan", 8, System::instance()->uid());
}

void System::init()
{
  _state = Initializing;
  _logInfo();
  NvsFlash::init();
  _initMacADDR();
  _loadData();
  _launchTasks();
  _state = Running;
}

void System::_logInfo()
{
  APP_LOGW("[System]", "esp flash encryption enabled: %s", flashEncryptionEnabled()? "Yes" : "No");
  APP_LOGW("[System]", "free RAM: %ld bytes", esp_get_free_heap_size());
  APP_LOGW("[System]", "data size: %d bytes", sizeof(_data));
}

// Low priority numbers denote low priority tasks. 
// The idle task has priority zero (tskIDLE_PRIORITY).
// configMAX_PRIORITIES defined in "FreeRTOSConfig.h"
// #define DISPLAY_TASK_PRIORITY               3
// #define DISPLAY_GUARD_TASK_PRIORITY         3
#define WIFI_TASK_PRIORITY                  3
#define SNTP_TASK_PRIORITY                  3
#define MQTTCLIENT_TASK_PRIORITY            3
#define HTTPSERVER_TASK_PRIORITY            3
// #define PM_SENSOR_TASK_PRIORITY             3
// #define CO2_SENSOR_TASK_PRIORITY            3
#define SHT3X_TASK_PRIORITY                 3
// #define TSL2561_TASK_PRIORITY               3
// #define ORIENTATION_TASK_PRIORITY           3
// #define STATUS_CHECK_TASK_PRIORITY          3
// #define TOUCH_PAD_TASK_PRIORITY             3
// #define BUZZER_TASK_PRIORITY                3
#define DAEMON_TASK_PRIORITY                10
#define HUMID_CONTROL_TASK_PRIORITY         3

#define PRO_CORE    0
#define APP_CORE    1

#define RUN_ON_CORE APP_CORE

// inline void _launchDisplayTask()
// {
//   xTaskCreatePinnedToCore(display_task, "display_task", 4096, NULL, DISPLAY_TASK_PRIORITY, &displayTaskHandle, PRO_CORE);
// }

void System::_launchTasks()
{
  beforeCreateTasks();

  // _launchDisplayTask();
  // xTaskCreatePinnedToCore(display_guard_task, "display_guard_task", 2048, NULL, DISPLAY_GUARD_TASK_PRIORITY, &displayGuardTaskHandle, PRO_CORE);

  xTaskCreatePinnedToCore(sht3x_sensor_task, "sht3x_sensor_task", 4096, NULL, SHT3X_TASK_PRIORITY, &sht3xSensorTaskHandle, RUN_ON_CORE);

  // xTaskCreatePinnedToCore(status_check_task, "status_check_task", 2048, NULL, STATUS_CHECK_TASK_PRIORITY, NULL, RUN_ON_CORE);

  xTaskCreate(&wifi_task, "wifi_connection_task", 4096, NULL, WIFI_TASK_PRIORITY, &wifiTaskHandle);
  vTaskDelay(100 / portTICK_PERIOD_MS);

  xTaskCreate(&sntp_task, "sntp_task", 4096, NULL, SNTP_TASK_PRIORITY, &sntpTaskHandle);
  vTaskDelay(100 / portTICK_PERIOD_MS);

  // if (_data.config1.deployMode == MQTTClientMode || _data.config1.deployMode == MQTTClientAndHTTPServerMode)
  //   xTaskCreatePinnedToCore(mqtt_task, "mqtt_task", 8192, NULL, MQTTCLIENT_TASK_PRIORITY, NULL, RUN_ON_CORE);
  // else if (_data.config1.deployMode == HTTPServerMode || _data.config1.deployMode == MQTTClientAndHTTPServerMode)
  //   xTaskCreatePinnedToCore(http_task, "http_task", 8192, NULL, HTTPSERVER_TASK_PRIORITY, NULL, RUN_ON_CORE);
  xTaskCreatePinnedToCore(http_task, "http_task", 8192, NULL, HTTPSERVER_TASK_PRIORITY, NULL, RUN_ON_CORE);
  vTaskDelay(100 / portTICK_PERIOD_MS);

  // xTaskCreate(&buzzer_task, "buzzer_task", 2048, NULL, BUZZER_TASK_PRIORITY, &buzzerTaskHandle);
  xTaskCreatePinnedToCore(daemon_task, "daemon_task", 2048, NULL, DAEMON_TASK_PRIORITY, NULL, RUN_ON_CORE);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  // daemon_task();

  xTaskCreate(&humid_control_task, "humid_control_task", 4096, NULL, HUMID_CONTROL_TASK_PRIORITY, &humidControlTaskHandle);
  // vTaskDelay(100 / portTICK_PERIOD_MS);
}

void System::pausePeripherals(const char *screenMsg)
{
  if (screenMsg) {
    // dc.setScreenMessage(screenMsg);
    // _hasScreenMessage = true;
  }

  _enablePeripheralTaskLoop = false;

  while (_statusTaskState == TaskRunning || _sht3xSensorTaskState == TaskRunning) {
    APP_LOGC("[System]", "sts: %d, sht: %d", _statusTaskState, _sht3xSensorTaskState);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    // APP_LOGC("[System]", "pause sync delay");
  }
}

void System::resumePeripherals()
{
  // dc.setScreenMessage(NULL);
  // if (_displayTaskState == TaskPaused)           _displayTaskState = TaskRunning;
  if (_statusTaskState == TaskPaused)            _statusTaskState = TaskRunning;
  // if (_pmSensorTaskState == TaskPaused)          _pmSensorTaskState = TaskRunning;
  // if (_co2SensorTaskState == TaskPaused)         _co2SensorTaskState = TaskRunning;
  if (_sht3xSensorTaskState == TaskPaused)       _sht3xSensorTaskState = TaskRunning;
  // if (_tsl2561SensorTaskState == TaskPaused)     _tsl2561SensorTaskState = TaskRunning;
  // if (_orientationSensorTaskState == TaskPaused) _orientationSensorTaskState = TaskRunning;
  _enablePeripheralTaskLoop = true;
}

void System::powerOff()
{
  APP_LOGC("[System]", "power off");

  // update maintenance upon restart
  _updateMaintenance();

  // save memory to data to flash before power off
  _saveMemoryData();

//     // power off
//   if (powerManager.powerOff()) {
// #ifdef DEBUG_APP_OK
//     APP_LOGC("[System]", "power off cmd succeeded");
// #endif
//   }
// #ifdef DEBUG_APP_ERR
//   else {
//     APP_LOGE("[System]", "pwer off cmd failed");
//   }
// #endif
}

void System::turnWifiOn(bool on)
{
  if (_data.config1.wifiOn != on) {
    _data.config1.wifiOn = on;
    _updateConfig1(); // _saveConfig1();
  }
}
void System::turnDisplayOn(bool on)
{
  // dc.turnOn(on);
}

void System::turnDisplayAutoAdjustOn(bool on)
{
  // if (_data.config1.displayAutoAdjustOn != on) {
  //   _data.config1.displayAutoAdjustOn = on;
  //   _updateConfig1(); // _saveConfig1();
  // }
}

void System::toggleWifi()
{
  _data.config1.wifiOn = !_data.config1.wifiOn;
  _updateConfig1(); // _saveConfig1();
}

void System::toggleDisplay()
{
  // dc.toggleDisplay();
}

void System::onEvent(int eventId)
{
  // switch (eventId) {
  //   case INPUT_EVENT_PWR_BTN_SHORT_RELEASE:
  //     if (alertSoundOn()) turnAlertSoundOn(false);
  //     break;

  //   case INPUT_EVENT_PWR_BTN_LONG_PRESS:
  //     powerOff();
  //     break;

  //   case INPUT_EVENT_USR_BTN_SHORT_RELEASE:
  //     // dc.onUsrButtonRelease();
  //     break;

  //   case INPUT_EVENT_USR_BTN_MEDIUM_RELEASE:
  //     toggleWifi();
  //     break;

  //   case INPUT_EVENT_USR_BTN_XLONG_PRESS:
  //     toggleDeployMode();
  //     break;

  //   case INPUT_EVENT_PWR_CHIP_INT:
  //     _hasPwrEvent = true;
  //     break;

  //   case INPUT_EVENT_MB_TEMP_CALIBRATE:
  //     setMbTempCalibration(true);
  //     turnAlertSoundOn(true);      // give sound response
  //     break;

  //   case SYS_EVENT_POWER_LOW:
  //     powerOff();
  //     break;

  //   default:
  //     break;
  // }
}

#define SYSTEM_DATA_TAG                   "appData"
// #define SYSTEM_CONFIG1_TAG                "appConf1"
// #define SYSTEM_CONFIG2_TAG                "appConf2"
// #define SYSTEM_MAINTENANCE_TAG            "appMaintenance"
// #define SYSTEM_RESET_RESTORE_TAG          "appResetRestore"
// #define SYSTEM_BIAS_TAG                   "appBias"
// #define ALERT_TAG                         "appAlerts"
// #define TOKEN_TAG                         "appTokens"

bool System::_loadData()
{
  return NvsFlash::loadData(SYSTEM_DATA_TAG, &_data, sizeof(_data));
}

bool System::_saveData()
{
  // calculate to update maintenance
  _calculateMaintenance();

  // save data
  bool succeeded = NvsFlash::saveData(SYSTEM_DATA_TAG, &_data, sizeof(_data));
  _dataNeedToSave = false; // ? or _dataNeedToSave = !succeeded;
  return succeeded;
}

void System::_updateData(bool saveImmedidately)
{
  if (saveImmedidately)
    _saveData();
  else
    _dataNeedToSave = true; // save when reboot or power off
}

void System::_calculateMaintenance()
{
  LifeTime sessionLife = (LifeTime)(esp_timer_get_time() / 1000000);
  _data.maintenance.allSessionsLife += (sessionLife - _currentSessionLife);
  _data.maintenance.recentSessionLife = sessionLife;
  _currentSessionLife = sessionLife;
  APP_LOGC("[System]", "session life: %ld, all sessions life: %ld", sessionLife, _data.maintenance.allSessionsLife);
}

void System::_saveMemoryData()
{
  // save those need to save ...
  if (_dataNeedToSave)  _saveData();
}

void System::_updateConfig1(bool saveImmedidately)
{
  _updateData(saveImmedidately);
}

void System::_updateConfig2(bool saveImmedidately)
{
  _updateData(saveImmedidately);
}

void System::_updateMaintenance(bool saveImmedidately)
{
  _updateData(saveImmedidately);
}

void System::_updateResetRestore(bool saveImmedidately)
{
  _updateData(saveImmedidately);
}

void System::_updateAlerts(bool saveImmedidately)
{
  _updateData(saveImmedidately);
}

void System::_updateMobileTokens(bool saveImmedidately)
{
  _updateData(saveImmedidately);
}

DeployMode System::deployMode()
{
  return _data.config1.deployMode;
}

SensorType System::pmSensorType()
{
  return _data.config2.pmSensorType;
}

SensorType System::co2SensorType()
{
  return _data.config2.co2SensorType;
}

uint32_t System::devCapability()
{
  return _data.config2.devCapability;
}

void System::setDeployMode(DeployMode mode)
{
  if (_data.config1.deployMode != mode) {
    _data.config1.deployMode = mode;
    _updateConfig1();
  }
}

void System::toggleDeployMode()
{
  if (_data.config1.deployMode == HTTPServerMode) {
    _data.config1.deployMode = MQTTClientMode;
  }
  else {
    _data.config1.deployMode = HTTPServerMode;
  }
  _data.config1.wifiOn = true; // when deploy mode changed, always turn on wifi
  _updateConfig1();
  restart();
}

void System::setSensorType(SensorType pmType, SensorType co2Type)
{
  // if (_data.config2.pmSensorType != pmType || _data.config2.co2SensorType != co2Type) {
  //   _data.config2.pmSensorType = pmType;
  //   _data.config2.co2SensorType = co2Type;
  //   _data.config2.devCapability = ( capabilityForSensorType(_data.config2.pmSensorType) |
  //                                   capabilityForSensorType(_data.config2.co2SensorType) );
  //   _data.config2.devCapability |= DEV_BUILD_IN_CAPABILITY_MASK;
  //   _data.config2.devCapability |= ORIENTATION_CAPABILITY_MASK;
  //   _data.config2.devCapability |= LUMINOSITY_CAPABILITY_MASK;
  //   _updateConfig2();
  // }
}

void System::setDevCapability(uint32_t cap)
{
  if (_data.config2.devCapability != cap) {
    _data.config2.devCapability = cap;
    _updateConfig2();
  }
}

const char* System::deviceName()
{
  return _data.config2.devName;
}

void System::setDeviceName(const char* name, size_t len)
{
  if (len > 0) {
    len = len < DEV_NAME_MAX_LEN ? len : DEV_NAME_MAX_LEN;
    memcpy(_data.config2.devName, name, len);
    _data.config2.devName[len] = '\0'; // null terminated
  }
  else {
    strncpy(_data.config2.devName, name, DEV_NAME_MAX_LEN);
  }
  _updateConfig2();
}

#ifdef DEBUG_BATTERY_LIFE
void System::clearMaintenance()
{
  _data.maintenance.allSessionsLife = 0;
  _data.maintenance.recentSessionLife = 0;
  _updateMaintenance(true);
}
#endif

const Maintenance * System::maintenance()
{
  _calculateMaintenance();
  return &_data.maintenance;
}

SysResetRestore * System::resetRestoreData()
{
  return &_data.resetRestore;
}
bool System::alertPnEnabled()
{
  return _data.alerts.pnEnabled;
}

bool System::alertSoundEnabled()
{
  return _data.alerts.soundEnabled;
}

Alerts * System::alerts()
{
  return &_data.alerts;
}

TriggerAlert System::sensorValueTriggerAlert(SensorDataType type, float value)
{
  if (_data.alerts.sensors[type].lEnabled && value < _data.alerts.sensors[type].lValue) return TriggerL;
  else if (_data.alerts.sensors[type].gEnabled && value >= _data.alerts.sensors[type].gValue) return TriggerG;
  else return TriggerNone;
}

MobileTokens * System::mobileTokens()
{
  return &_data.mobileTokens;
}

bool System::tokenEnabled(MobileOS os, const char* token)
{
  int8_t index = _data.mobileTokens.findToken(token);
  return (index != -1 && _data.mobileTokens.token(index).os == os && _data.mobileTokens.token(index).on);
}

void System::setAlertPnEnabled(bool enabled)
{
  if (_data.alerts.pnEnabled != enabled) {
    _data.alerts.pnEnabled = enabled;
    _dataNeedToSave = true;
#ifdef ENABLE_MQTT_TASK
    if (enabled) _resetAlertReactiveCounter();
#endif
  }
}

void System::setAlertSoundEnabled(bool enabled)
{
  if (_data.alerts.soundEnabled != enabled) {
    _data.alerts.soundEnabled = enabled;
    _dataNeedToSave = true;
    if (!enabled) turnAlertSoundOn(false);
  }
}

bool System::alertSoundOn()
{
  return false;
  // return _buzzerTriggered;
}

void System::turnAlertSoundOn(bool on)
{
  // if (_buzzerTriggered != on) {
  //   if (on) {
  //     _buzzerTriggered = true;
  //     _buzzerSoundTicksCounter = 0;
  //   }
  //   else {
  //     _buzzerTriggered = false;
  //     buzzer.stop();
  //   }
  // }
}

void System::setAlert(SensorDataType type, bool lEnabled, bool gEnabled, float lValue, float gValue)
{
  _data.alerts.sensors[type].lEnabled = lEnabled;
  _data.alerts.sensors[type].gEnabled = gEnabled;
  _data.alerts.sensors[type].lValue = lValue;
  _data.alerts.sensors[type].gValue = gValue;
  _dataNeedToSave = true;
}

void System::setPnToken(bool enabled, MobileOS os, const char *token, size_t groupLen, const char *group)
{
  _data.mobileTokens.setToken(enabled, os, token, groupLen, group);
  _dataNeedToSave = true;
}

void System::resetAlertReactiveCounter()
{
#ifdef ENABLE_MQTT_TASK
  _resetAlertReactiveCounter();
#endif
}

FanMode * System::fanMode()
{
  return &_data.fanMode;
}

void System::setFanMode(uint8_t mode, float arg1, float arg2)
{
  _data.fanMode.mode = mode;
  if (mode == FAN_MODE_AUTO) { _data.fanMode.lthr = arg1; _data.fanMode.gthr = arg2; }
  else if (mode == FAN_MODE_DELAY) { _data.fanMode.min = arg1; }
  APP_LOGC("[System]", "setFanMode: %d, %.1f, %.1f", mode, arg1, arg2);

  _currentMode = FAN_MODE_UNDEFINED; // cause humid task update
  _updateData(true);
}

const char* System::firmwareName()
{
  return FIRMWARE_NAME;
}

const char* System::uid()
{
  return macAddress();
}

const char* System::macAddress()
{
  if (MAC_ADDR[12] == 0xFF)
    _initMacADDR();
  return MAC_ADDR;
}

const char* System::idfVersion()
{
  return esp_get_idf_version();
}

const char* System::firmwareVersion()
{
  // return 0;
  return FIRMWARE_VERSION_STR;
}

const char* System::boardVersion()
{
  return BOARD_VERSION_STR;
}

static char MODEL[20];

const char* System::model()
{
  // sprintf(MODEL, sensorTypeStr(_data.config2.pmSensorType));
  // sprintf(MODEL+strlen(MODEL), "-");
  // sprintf(MODEL+strlen(MODEL), sensorTypeStr(_data.config2.co2SensorType));
  MODEL[0] = '1'; MODEL[1] = '\0';
  return MODEL;
}

bool System::flashEncryptionEnabled()
{
  return esp_flash_encryption_enabled();
}

void System::setDebugFlag(uint8_t flag)
{
  // _debugFlag = flag;
}

void System::restoreFactory()
{
    const esp_partition_t*    fp ;
    esp_err_t                 err ;
    fp = esp_partition_find_first(ESP_PARTITION_TYPE_APP,             // try get factory partition
                                  ESP_PARTITION_SUBTYPE_APP_FACTORY,
                                  "factory" );
    if (fp) {
      err = esp_ota_set_boot_partition(fp);                           // set partition for boot
      if (err == ESP_OK) {
        setRestartRequest();
      } else {
        APP_LOGE("[System]", "restore factory partition failed");
      }
    }
}

void System::deepSleepReset()
{
  // save alert reactive counter for later restore
  _data.resetRestore.deepSleepResetCount++;
#ifdef ENABLE_MQTT_TASK
  _data.resetRestore.lAlertReactiveCounter = _lAlertReactiveCounter;
  _data.resetRestore.gAlertReactiveCounter = _gAlertReactiveCounter;
#endif
  _updateResetRestore();

  // update maintenance upon restart
  _updateMaintenance();

  // save memory data
  _saveMemoryData();

  // stop those need to stop ...
  pausePeripherals("prepare to deep sleep reset ...");

  _state = Restarting;
  esp_deep_sleep(50000);
}

void System::setRestartRequest()
{
  _hasRebootRequest = true;
}

void System::setWebsocketClientOpRequest(int op)
{
  if (op == 1)
    _hasWebsocketClientStopRequest = true;
  else if (op == 2)
    _hasWebsocketClientReinitRequest = true;
}

void System::setHttpServerStopRequest()
{
  _hasHttpServerStopRequest = true;
}

void System::setAllMessageProtocolsStopRequest()
{
  _hasHttpServerStopRequest = true;
  _hasWebsocketClientStopRequest = true;
}

void System::restart()
{
  // update maintenance upon restart
  _updateMaintenance();

  // save memory data
  _saveMemoryData();

  // stop those need to stop ...
  pausePeripherals("prepare to reboot ...");

  _state = Restarting;
  esp_restart();
}

bool System::restarting()
{
  return _state == Restarting;
}
