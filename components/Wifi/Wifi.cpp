/*
 * Wifi: Wrap the esp wifi
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "Wifi.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_netif.h"
// #include "esp_netif_types.h"

#include "AppLog.h"
#include "System.h"

/////////////////////////////////////////////////////////////////////////////////////////
// ------ enterprise AP
#ifdef ENABLE_EAP
/* CA cert, taken from wpa2_ca.pem
   Client cert, taken from wpa2_client.crt
   Client key, taken from wpa2_client.key

   The PEM, CRT and KEY file were provided by the person or organization
   who configured the AP with wpa2 enterprise.

   To embed it in the app binary, the PEM, CRT and KEY file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern uint8_t ca_pem_start[] asm("_binary_wpa2_ca_pem_start");
extern uint8_t ca_pem_end[]   asm("_binary_wpa2_ca_pem_end");
extern uint8_t client_crt_start[] asm("_binary_wpa2_client_crt_start");
extern uint8_t client_crt_end[]   asm("_binary_wpa2_client_crt_end");
extern uint8_t client_key_start[] asm("_binary_wpa2_client_key_start");
extern uint8_t client_key_end[]   asm("_binary_wpa2_client_key_end");

#endif // ENABLE_EAP

/////////////////////////////////////////////////////////////////////////////////////////
// ------ default WIFI AP, STA. In case of any init error this API aborts.
static esp_netif_t *_defaultWifiApInstance = NULL;
static esp_netif_t *_defaultWifiStaInstance = NULL;

static esp_netif_t * _defaultWifiAp() { 
  if (!_defaultWifiApInstance)  APP_LOGE("[Wifi]", "_defaultWifiApInstance NULL");
  return _defaultWifiApInstance; }
static esp_netif_t * _defaultWifiSta() { 
    if (!_defaultWifiStaInstance) APP_LOGE("[Wifi]", "_defaultWifiStaInstance NULL");
return _defaultWifiStaInstance; }

static esp_netif_t * _createDefaultWifiAp() {
  if (_defaultWifiApInstance != NULL) esp_netif_destroy(_defaultWifiApInstance);
  _defaultWifiApInstance = esp_netif_create_default_wifi_ap();
  return _defaultWifiApInstance;
}

static esp_netif_t * _createDefaultWifiSta() {
  if (_defaultWifiStaInstance != NULL) esp_netif_destroy(_defaultWifiStaInstance);
  _defaultWifiStaInstance = esp_netif_create_default_wifi_sta();
  return _defaultWifiStaInstance;
}

static void _destroyDefaultWifiAp() {
  if (_defaultWifiApInstance != NULL) {
    esp_netif_destroy(_defaultWifiApInstance);
    _defaultWifiApInstance = NULL;
  }
}

static void _destroyDefaultWifiSta() {
  if (_defaultWifiStaInstance != NULL) {
    esp_netif_destroy(_defaultWifiStaInstance);
    _defaultWifiStaInstance = NULL;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
// ------ helper
inline size_t stringLen(const char *str)
{
  return strlen((const char*)str);
}

inline void stringAssign(char *target, const char *str, size_t len)
{
  target[0] = '\0';
  strncat((char *)target, (const char*)str, len);
}

/////////////////////////////////////////////////////////////////////////////////////////
// ------ static shared instance
static Wifi _wifiInstance;

Wifi * Wifi::instance()
{
  return &_wifiInstance;
}

/////////////////////////////////////////////////////////////////////////////////////////
// ------ contructor
Wifi::Wifi()
: _initialized(false)
, _started(false)
, _connected(false)
, _autoreconnect(true)
, _apStaConnectionCount(0)
, _nextAltApIndex(0)
, _connectionFailCount(0)
, _altApsConnectionFailRound(0)
{
  // set default config value
  _config.mode = WIFI_MODE_AP;
  _config.powerSaveType = WIFI_PS_MIN_MODEM;
#ifdef ENABLE_EAP
  _config.eapConfig.enabled = false;
  _config.eapConfig.eapMode = EAP_PEAP;
#endif
  _config.altApsHead = 0;
  _config.altApCount = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// ------ config
void Wifi::setDefaultConfig()
{
  char apName[48];
  sprintf(apName, "%s_%.*s", "DishwasherDehumidifier", 12, System::instance()->uid());

  setWifiMode(WIFI_MODE_APSTA);
  setStaConfig("ssid", "ssidpasswd", false, false);
  setApConfig(apName, "Dehumidifier");
  setHostName(apName);
#ifdef ENABLE_EAP
  enableEap(false);
  setEapConfig("eapid", "eapusername", "eapuserpassword");
#endif
  _config.altApsHead = 0;
  _config.altApCount = 0;
  _nextAltApIndex = 0;
}

void Wifi::setWifiMode(wifi_mode_t mode)
{
  if (!_initialized)
    _config.mode = mode;
}

void Wifi::setPowerSaveEnabled(bool enabled)
{
  _config.powerSaveType = enabled ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE;
  esp_wifi_set_ps(_config.powerSaveType);
}

bool _setConfSsidPass(uint8_t *ssidT, const char *ssidS, size_t ssidMaxLen,
                      uint8_t *passT, const char *passS, size_t passMaxLen)
{
  size_t ssidLen = stringLen(ssidS);
  size_t passLen = stringLen(passS);

  if (ssidLen > ssidMaxLen || passLen > passMaxLen) return false;

  stringAssign((char*)ssidT, ssidS, ssidLen);
  stringAssign((char*)passT, passS, passLen);
  return true;
}

bool Wifi::setStaConfig(const char *ssid, const char *passwd, bool forceOverride, bool append)
{
  if (!forceOverride && _initialized) return false;

  if (append)
    appendAltApConnectionSsidPassword(ssid, passwd);

  return _setConfSsidPass(_config.staConfig.sta.ssid, ssid,
                          sizeof(_config.staConfig.sta.ssid),
                          _config.staConfig.sta.password, passwd,
                          sizeof(_config.staConfig.sta.password));
}

bool Wifi::setApConfig(const char      *ssid,
                       const char      *passwd,
                       bool             forceOverride,
                       wifi_auth_mode_t authmode,
                       uint8_t          maxConnection,
                       uint8_t          ssidHidden)
{
  if (!forceOverride && _initialized) return false;

  if ( _setConfSsidPass(_config.apConfig.ap.ssid, ssid,
                        sizeof(_config.apConfig.ap.ssid),
                        _config.apConfig.ap.password, passwd,
                        sizeof(_config.apConfig.ap.password)) ) {

    _config.apConfig.ap.ssid_len = stringLen(ssid);
    _config.apConfig.ap.authmode = authmode;
    _config.apConfig.ap.max_connection = maxConnection;
    _config.apConfig.ap.ssid_hidden = ssidHidden;
    return true;
  }

  return false;
}

bool Wifi::setAltApConnectionSsidPassword(const char *ssid, const char *passwd, uint8_t index)
{
  if (index >= ALTERNATIVE_AP_LIST_SIZE) return false;

  index += _config.altApsHead;
  index = index % ALTERNATIVE_AP_LIST_SIZE;

    return _setConfSsidPass(_config.altAps[index].ssid, ssid,
                            sizeof(_config.staConfig.sta.ssid),
                            _config.altAps[index].password, passwd,
                            sizeof(_config.staConfig.sta.password));
}

bool Wifi::appendAltApConnectionSsidPassword(const char *ssid, const char *passwd)
{
  // circular move ahead
  uint8_t index = (ALTERNATIVE_AP_LIST_SIZE + _config.altApsHead - 1) % ALTERNATIVE_AP_LIST_SIZE;

  if ( _setConfSsidPass(_config.altAps[index].ssid, ssid,
                        sizeof(_config.staConfig.sta.ssid),
                        _config.altAps[index].password, passwd,
                        sizeof(_config.staConfig.sta.password)) ) {

    _config.altApsHead = index;
    _nextAltApIndex = 0; // update RAM next Alt index, point to head (head + 0)

    if (_config.altApCount < ALTERNATIVE_AP_LIST_SIZE)
      ++_config.altApCount;

    return true;
  }

  return false;
}

void Wifi::loadNextAltSsidPassword()
{
  if (_config.altApCount > 0) {
    if (_config.mode == WIFI_MODE_APSTA || _config.mode == WIFI_MODE_STA) {
      uint8_t index = (_config.altApsHead + _nextAltApIndex) % ALTERNATIVE_AP_LIST_SIZE;
      APP_LOGC("[Wifi]", "load alt ssid: %s, pass: %s",
               (const char*)_config.altAps[index].ssid,
               (const char*)_config.altAps[index].password);
      _setConfSsidPass(_config.staConfig.sta.ssid,
                       (const char*)_config.altAps[index].ssid,
                       sizeof(_config.staConfig.sta.ssid),
                       _config.staConfig.sta.password,
                       (const char*)_config.altAps[index].password,
                       sizeof(_config.staConfig.sta.password));

      ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &_config.staConfig) );
      _nextAltApIndex = (_nextAltApIndex + 1) % _config.altApCount;
    }
  }
}

void Wifi::clearAltApConnectionSsidPassword()
{
  _config.altApCount = 1; // mark only head valid
}

void Wifi::getAltApConnectionSsidPassword(SsidPasswd *&list, uint8_t &head, uint8_t &count)
{
  list = _config.altAps;
  head = _config.altApsHead;
  count = _config.altApCount;
}

#ifdef ENABLE_EAP

bool Wifi::setEapConfig(const char *eapId,
                        const char *username,
                        const char *password,
                        EapMode     mode)
{
  if (_initialized) return false;

  size_t eapIdLen = stringLen(eapId);
  size_t usernameLen = stringLen(username);
  size_t passwdLen = stringLen(password);

  if (eapIdLen > EAP_ID_MAX_LEN ||
    usernameLen > EAP_USERNAME_MAX_LEN ||
    passwdLen < 8 ||
    passwdLen > EAP_PASSWORD_MAX_LEN)
    return false;

  _config.eapConfig.eapMode = mode;
  stringAssign(_config.eapConfig.eapId, eapId, eapIdLen);
  stringAssign(_config.eapConfig.eapUsername, username, usernameLen);
  stringAssign(_config.eapConfig.eapPassword, password, passwdLen);

  return true;
}

void Wifi::_initEap()
{
  if (_config.eapConfig.enabled) {
    unsigned int ca_pem_bytes = ca_pem_end - ca_pem_start;
    unsigned int client_crt_bytes = client_crt_end - client_crt_start;
    unsigned int client_key_bytes = client_key_end - client_key_start;

    ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_ca_cert(ca_pem_start, ca_pem_bytes) );
    ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_cert_key(client_crt_start, client_crt_bytes,
                                                        client_key_start, client_key_bytes, NULL, 0) );

    ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)_config.eapConfig.eapId,
                                                         stringLen(_config.eapConfig.eapId)) );
    if (_config.eapConfig.eapMode == EAP_PEAP || _config.eapConfig.eapMode == EAP_TTLS) {
      ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_username((uint8_t*)_config.eapConfig.eapUsername,
                                                           stringLen(_config.eapConfig.eapUsername)) );
      ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_password((uint8_t*)_config.eapConfig.eapPassword,
                                                           stringLen(_config.eapConfig.eapPassword)) );
    }
    ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_enable() );
  }
}

#endif

// host name can be changed at Framework level:
// esp-idf/components/lwip/port/netif/ethernetif.c:213:  netif->hostname = "espressif";
// esp-idf/components/lwip/port/netif/wlanif.c:202:  netif->hostname = "espressif";
bool Wifi::setHostName(const char* hostname)
{
  size_t len = stringLen(hostname);
  if (len > HOST_NAME_MAX_LEN || len == 0) return false;

  stringAssign(_config.hostName, hostname, len);
  if ( _started ) {
    if (_config.mode == WIFI_MODE_APSTA || _config.mode == WIFI_MODE_STA) {
      if (_defaultWifiSta()) ESP_ERROR_CHECK( esp_netif_set_hostname(_defaultWifiSta(), _config.hostName) );
    }
    if (_config.mode == WIFI_MODE_APSTA || _config.mode == WIFI_MODE_AP) {
      if (_defaultWifiAp()) ESP_ERROR_CHECK( esp_netif_set_hostname(_defaultWifiAp(), _config.hostName) );
    }
  }

  return true;
}

const char * Wifi::getHostName()
{
  const char *hostname = NULL;
  if (_config.mode == WIFI_MODE_STA || _config.mode == WIFI_MODE_APSTA) {
    if (_defaultWifiSta()) ESP_ERROR_CHECK( esp_netif_get_hostname(_defaultWifiSta(), &hostname) );
  }
  else if (_config.mode == WIFI_MODE_AP) {
    if (_defaultWifiAp()) ESP_ERROR_CHECK( esp_netif_get_hostname(_defaultWifiAp(), &hostname) );
  }
  return hostname;
}

const char * Wifi::staSsid()
{
  return (const char *)_config.staConfig.sta.ssid;
}

const char * Wifi::staPassword()
{
  return (const char *)_config.staConfig.sta.password;
}

const char * Wifi::apSsid()
{
  return (const char *)_config.apConfig.ap.ssid;
}

const char * Wifi::apPassword()
{
  return (const char *)_config.apConfig.ap.password;
}

/////////////////////////////////////////////////////////////////////////////////////////
// ------ wifi init and event handler
/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t           _wifiEventGroup;

// event handler instance object related to the registered event handler and data, can be NULL
static esp_event_handler_instance_t _instanceAnyId;
static esp_event_handler_instance_t _instanceGotIp;

/* The event group allows multiple bits for each event, but we only care about two event
   - we are connected to the AP with an IP
   - we failed to connect after the maximum amount of retries */
static const int CONNECTED_BIT = BIT0;
static const int FAIL_BIT      = BIT1;

static void wifi_app_event_handler( void* ctx, esp_event_base_t eventBase,
                                    int32_t eventId, void* eventData)
{
  if (eventBase == WIFI_EVENT) {
    wifi_event_t wifiEvent = static_cast<wifi_event_t>(eventId);
    switch (wifiEvent) {

      case WIFI_EVENT_STA_START: {
        Wifi::instance()->onStaStart();
      } break;

      case WIFI_EVENT_STA_STOP: {
        Wifi::instance()->onStaStop();
      } break;

      case WIFI_EVENT_STA_CONNECTED: {
        Wifi::instance()->onStaConnected();
      } break;

      case WIFI_EVENT_STA_DISCONNECTED: {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) eventData;
        Wifi::instance()->onStaDisconnected(event->reason);
      } break;

      case WIFI_EVENT_AP_START: {
        Wifi::instance()->onApStart();
      } break;

      // case WIFI_EVENT_AP_STOP:
      //   break;

      case WIFI_EVENT_AP_STACONNECTED: {
        Wifi::instance()->onApStaConnected();
      } break;

      case WIFI_EVENT_AP_STADISCONNECTED: {
        Wifi::instance()->onApStaDisconnected();
      } break;

      default:
        break;
    }
  } 
  else if (eventBase == IP_EVENT) {
    ip_event_t ipEvent = static_cast<ip_event_t>(eventId);
    switch (ipEvent) {
      case IP_EVENT_STA_GOT_IP: {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) eventData;
        Wifi::instance()->onStaGotIp(event->ip_info.ip.addr);
      } break;

      // case IP_STA_LOST_IP:
      //   break;

      default:
        break;
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
// ------ event handler
void Wifi::onScanDone()
{
}

void Wifi::onStaStart()
{
  APP_LOGC("[Wifi]", "try to connect to \"%s\"", _config.staConfig.sta.ssid);
  ESP_ERROR_CHECK( esp_wifi_connect() );
}

void Wifi::onStaGotIp(uint32_t ip)
{
  APP_LOGC("[Wifi]", "connected, got ip: %d.%d.%d.%d", uint8_t(ip), uint8_t(ip>>8),
                                                       uint8_t(ip>>16), uint8_t(ip>>24));
  xEventGroupSetBits(_wifiEventGroup, CONNECTED_BIT);
  _connectionFailCount = 0;
  _altApsConnectionFailRound = 0;
  _connected = true;

  if ( (_config.mode == WIFI_MODE_APSTA || _config.mode == WIFI_MODE_STA) && _config.hostName[0] != '\0') {
    ESP_ERROR_CHECK( esp_netif_set_hostname(_defaultWifiSta(), _config.hostName) );
  }
}

void Wifi::onStaStop()
{
}

void Wifi::onStaConnected()
{
}

#define TRY_OTHER_AP_AFTER_FAIL_COUNT  3
#define MAX_ALT_APS_TRY_ROUND          1000

void Wifi::onStaDisconnected(uint8_t reason)
{
  APP_LOGI("[Wifi]", "disconnected, reason code: %d", reason);
  xEventGroupClearBits(_wifiEventGroup, CONNECTED_BIT);
  _connected = false;
  bool tryReconnect = true;
  switch(reason) {
    // case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: // legacy code
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_NO_AP_FOUND:
      ++_connectionFailCount;
      break;
    case WIFI_REASON_ASSOC_LEAVE:
      tryReconnect = false;
      break;
    default:
      break;
  }

  if (!tryReconnect) return;

  if (_connectionFailCount >= TRY_OTHER_AP_AFTER_FAIL_COUNT) {
    if (_nextAltApIndex == _config.altApCount - 1) ++_altApsConnectionFailRound;
    if (_altApsConnectionFailRound < MAX_ALT_APS_TRY_ROUND) {
      loadNextAltSsidPassword();
      _connectionFailCount = 0;
    }
  }
  if (!System::instance()->restarting() && _altApsConnectionFailRound < MAX_ALT_APS_TRY_ROUND
      && _autoreconnect) {
    if (_started) {
      // ESP_ERROR_CHECK( esp_wifi_connect() );
      esp_err_t ret = esp_wifi_connect();
      if (ret != ESP_OK) {
        if (ret == ESP_ERR_WIFI_NOT_INIT)
          APP_LOGE("[Wifi]", "esp_wifi_connect err: ESP_ERR_WIFI_NOT_INIT");
        else if (ret == ESP_ERR_WIFI_NOT_STARTED)
          APP_LOGE("[Wifi]", "esp_wifi_connect err: ESP_ERR_WIFI_NOT_START");
      }
    }
  }
}

void Wifi::onApStart()
{
  APP_LOGI("[Wifi]", "AP start");
  if ( (_config.mode == WIFI_MODE_AP) && _config.hostName[0] != '\0') {
    ESP_ERROR_CHECK( esp_netif_set_hostname(_defaultWifiAp(), _config.hostName) );
  }
}

void Wifi::onApStaConnected()
{
  ++_apStaConnectionCount;
  // APP_LOGC("[Wifi]", "++_apStaConnectionCount %d", _apStaConnectionCount);
}

void Wifi::onApStaDisconnected()
{
  _apStaConnectionCount = 0;
  // APP_LOGC("[Wifi]", "--_apStaConnectionCount %d", _apStaConnectionCount);
}

void Wifi::waitConnected()
{
  xEventGroupWaitBits(_wifiEventGroup, CONNECTED_BIT, false, true, portMAX_DELAY);
}

bool Wifi::connected()
{
  return _connected;
}

bool Wifi::apStaConnected()
{
  return _apStaConnectionCount > 0;
}

bool Wifi::started()
{
  return _started;
}

/////////////////////////////////////////////////////////////////////////////////////////
// ------ connect, disconnect
void Wifi::connect(bool autoreconnect)
{
  _autoreconnect = autoreconnect;
  if (_started && !_connected) {
    ESP_ERROR_CHECK( esp_wifi_connect() );
  }
}

void Wifi::disconnect()
{
  if (_started && _connected) {
    _autoreconnect = false;
    ESP_ERROR_CHECK( esp_wifi_disconnect() );
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
// ------ init, deinit
void Wifi::init()
{
  if (_initialized) return;

  _wifiEventGroup = xEventGroupCreate();

  ESP_ERROR_CHECK( esp_netif_init() );
  ESP_ERROR_CHECK( esp_event_loop_create_default() );

  if (_config.mode == WIFI_MODE_APSTA || _config.mode == WIFI_MODE_STA) {
    // create default wifi sta
    _createDefaultWifiSta();
  }
  if (_config.mode == WIFI_MODE_APSTA || _config.mode == WIFI_MODE_AP) {
    // create default wifi ap
    _createDefaultWifiAp();

    // network ip
    esp_netif_ip_info_t info = { 0, 0, 0};
    info.ip.addr =      esp_netif_htonl(esp_netif_ip4_makeu32(192, 168, 4, 1));
    info.gw.addr =      esp_netif_htonl(esp_netif_ip4_makeu32(192, 168, 4, 1));
    info.netmask.addr = esp_netif_htonl(esp_netif_ip4_makeu32(255, 255, 255, 0));
    ESP_ERROR_CHECK( esp_netif_dhcps_stop(_defaultWifiAp()) );
    ESP_ERROR_CHECK( esp_netif_set_ip_info(_defaultWifiAp(), &info) );
    ESP_ERROR_CHECK( esp_netif_dhcps_start(_defaultWifiAp()) );
  }

  // init wifi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

  // register event handler
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_app_event_handler,
                                                      NULL,
                                                      &_instanceAnyId));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &wifi_app_event_handler,
                                                      NULL,
                                                      &_instanceGotIp));

  // set power save mode
  esp_err_t ret = esp_wifi_set_ps(_config.powerSaveType);
  APP_LOGI("[Wifi]", "init with power save support: %s", ret != ESP_ERR_NOT_SUPPORTED ? "Yes" : "No");
  
  // set wifi mode: ap or sta
  APP_LOGI("[Wifi]", "init with mode %d", _config.mode);
  ESP_ERROR_CHECK( esp_wifi_set_mode(_config.mode) );

  // set wifi mode config accordingly
  if (_config.mode == WIFI_MODE_APSTA || _config.mode == WIFI_MODE_STA)
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &_config.staConfig) );
  if (_config.mode == WIFI_MODE_APSTA || _config.mode == WIFI_MODE_AP)
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_AP, &_config.apConfig) );

#ifdef ENABLE_EAP
  _initEap();
#endif

  _initialized = true;
}

void Wifi::deinit()
{
  if (_initialized) {
    // event will not be processed after unregister
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT,   IP_EVENT_STA_GOT_IP, _instanceGotIp));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,    _instanceAnyId));

    // deinit wifi
    ESP_ERROR_CHECK( esp_wifi_deinit() );

    // destroys the esp_netif object accordingly
    if (_config.mode == WIFI_MODE_APSTA || _config.mode == WIFI_MODE_STA) {
      _destroyDefaultWifiSta();
    }
    if (_config.mode == WIFI_MODE_APSTA || _config.mode == WIFI_MODE_AP) {
      _destroyDefaultWifiAp();
    }

    // ESP_ERROR_CHECK( esp_netif_deinit() ); // deinitialization is not supported yet

    vEventGroupDelete(_wifiEventGroup);

    _initialized = false;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
// ------ start, stop
void Wifi::start(bool waitConnected)
{
  if (!_started) {
    _nextAltApIndex = 0;
    _connectionFailCount = 0;
    _altApsConnectionFailRound = 0;
    APP_LOGC("[Wifi]", "start wifi");
    ESP_ERROR_CHECK( esp_wifi_start() );
    if (waitConnected) this->waitConnected();
    // if ( (_config.mode == WIFI_MODE_APSTA || _config.mode == WIFI_MODE_STA) && _config.hostName[0] != '\0') {
    //   if (!waitConnected) this->waitConnected();  // wait connect anyway
    //   ESP_ERROR_CHECK( esp_netif_set_hostname(_defaultWifiSta(), _config.hostName) );
    // }
    _started = true;
  }
}

void Wifi::stop()
{
  if (_started) {
    APP_LOGC("[Wifi]", "stop wifi");
    if (_connected) {
      ESP_ERROR_CHECK( esp_wifi_disconnect() );
    }
    while (_connected) vTaskDelay(100/portTICK_PERIOD_MS);
    ESP_ERROR_CHECK( esp_wifi_stop() );
    _started = false;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
// ------ load, save config
#include "NvsFlash.h"
#define WIFI_STORAGE_NAMESPACE          "wifi"
#define WIFI_CONFIG_TAG                 "wifiConf"
#define WIFI_CONFIG_SAVE_COUNT_TAG      "wifiConfSC"  // wifi config save count

bool Wifi::loadConfig()
{
  bool succeeded = NvsFlash::loadData(WIFI_CONFIG_TAG, &_config, sizeof(_config), WIFI_CONFIG_SAVE_COUNT_TAG);
  if (succeeded) _nextAltApIndex = 0; // point to _config.altApsHead (head + nextIndex)
  return succeeded;
}

bool Wifi::saveConfig()
{
  return NvsFlash::saveData(WIFI_CONFIG_TAG, &_config, sizeof(_config), WIFI_CONFIG_SAVE_COUNT_TAG);
}
