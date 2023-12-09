/*
 * Wifi: Wrap the esp wifi
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _WIFI_H
#define _WIFI_H

#include "esp_wifi_types.h"
#include "esp_event.h"

/////////////////////////////////////////////////////////////////////////////////////////
// Wifi config
/////////////////////////////////////////////////////////////////////////////////////////
// ------ AP
#define WIFI_DEFAULT_AP_MAX_CONNECTION 4

// ------ EAP
// #define ENABLE_EAP // enable eap will occupy about 50K more bytes
#ifdef ENABLE_EAP
#define EAP_ID_MAX_LEN                 127
#define EAP_USERNAME_MAX_LEN           32
#define EAP_PASSWORD_MAX_LEN           64
enum EapMode {
    EAP_NULL   = 0,
    EAP_PEAP,
    EAP_TTLS
};
// ------ EAP config
struct WifiEapConfig {
    bool                     enabled;
    EapMode                  eapMode;
    char                     eapId[EAP_ID_MAX_LEN];
    char                     eapUsername[EAP_USERNAME_MAX_LEN];
    char                     eapPassword[EAP_PASSWORD_MAX_LEN];
};
#endif

// ------ host info
#define HOST_NAME_MAX_LEN              32
#define ALTERNATIVE_AP_LIST_SIZE       5

// ------ wifi config
struct SsidPasswd {
    uint8_t ssid[32];
    uint8_t password[64];
};

struct WifiConfig {
    wifi_mode_t              mode;
    wifi_ps_type_t           powerSaveType;
    wifi_config_t            apConfig;
    wifi_config_t            staConfig;
#ifdef ENABLE_EAP
    WifiEapConfig            eapConfig;
#endif
    uint8_t                  altApsHead;                       // head of circular list
    uint8_t                  altApCount;                       // alternative AP count
    SsidPasswd               altAps[ALTERNATIVE_AP_LIST_SIZE]; // alternative AP circular list
    char                     hostName[HOST_NAME_MAX_LEN+1];
};


/////////////////////////////////////////////////////////////////////////////////////////
// Wifi class
/////////////////////////////////////////////////////////////////////////////////////////
#undef connect // mongoose lib define 'connect' macro

class Wifi
{
public:
    // singleton
    static Wifi * instance();

public:
    // constructor
    Wifi();

    // config
    void setDefaultConfig();
    void setWifiMode(wifi_mode_t mode);
    void setPowerSaveEnabled(bool enabled = true);
    bool setStaConfig(const char *ssid, const char *passwd, bool forceOverride = false, bool append = true);
    bool setApConfig(const char      *ssid,
                     const char      *passwd,
                     bool forceOverride = false,
                     wifi_auth_mode_t authmode = WIFI_AUTH_WPA_WPA2_PSK,
                     uint8_t          maxConnection = WIFI_DEFAULT_AP_MAX_CONNECTION,
                     uint8_t          ssidHidden = 0); // default 0: not hidden

    // alternative AP connection ssid and password
    bool setAltApConnectionSsidPassword(const char *ssid, const char *passwd, uint8_t index);
    bool appendAltApConnectionSsidPassword(const char *ssid, const char *passwd);
    void loadNextAltSsidPassword();
    void clearAltApConnectionSsidPassword();
    void getAltApConnectionSsidPassword(SsidPasswd *&list, uint8_t &head, uint8_t &count);

#ifdef ENABLE_EAP
public:
    void enableEap(bool enabled = true) { _config.eapConfig.enabled = enabled; }
    bool setEapConfig(const char    *eapId,
                      const char    *username,
                      const char    *password,
                      EapMode        mode = EAP_PEAP);
protected:
    void _initEap();
#endif

public:
    bool setHostName(const char* hostname);
    const char * getHostName();
    void setAutoReconnect(bool autoreconnect = true) { _autoreconnect = autoreconnect; }

    const char * staSsid();
    const char * staPassword();
    const char * apSsid();
    const char * apPassword();

    // init, deinit
    void init();
    void deinit();

    // start, stop, connect, disconnect, status
    void start(bool waitConnected = false);
    void stop();
    void connect(bool autoreconnect = true);
    void disconnect();
    void waitConnected();
    bool connected();
    bool apStaConnected();
    bool started();

    // storage load, save
    bool loadConfig();
    bool saveConfig();

public:
    // event handler
    void onScanDone();
    void onStaStart();
    void onStaStop();
    void onStaConnected();
    void onStaDisconnected(uint8_t reason);
    void onStaGotIp(uint32_t ip);
    void onApStart();
    void onApStaConnected();
    void onApStaDisconnected();

protected:
    // flags
    bool                     _initialized;
    bool                     _started;
    bool                     _connected;
    bool                     _autoreconnect;
    int16_t                  _apStaConnectionCount;
    uint8_t                  _nextAltApIndex;
    uint16_t                 _connectionFailCount;
    uint16_t                 _altApsConnectionFailRound;
    // config
    WifiConfig               _config;
};

#endif // _WIFI_H
