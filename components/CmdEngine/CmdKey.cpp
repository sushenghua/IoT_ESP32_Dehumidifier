/*
 * CmdKey command key
 * Copyright (c) 2016 Shenghua Su
 *
 */

#include "CmdKey.h"

#include <string.h>

static const char * const CmdKeyStr[] = {
    "DoNothing",                // 0
    "GetSensorData",            // 1
    "GetSensorCapability",      // 2
    "GetDeviceInfo",            // 3
    "GetUID",                   // 4
    "GetFirmwareVersion",       // 5
    "GetIdfVersion",            // 6
    "GetHostname",              // 7
    "SetHostname",              // 8
    "GetDeviceName",            // 9
    "SetDeviceName",            // 10
    "GetStaSsidPass",           // 11
    "SetStaSsidPass",           // 12
    "GetApSsidPass",            // 13
    "SetApSsidPass",            // 14
    "GetAltApSsidPassList",     // 15
    "AppendAltApSsidPass",      // 16
    "ClearAltApSsidPassList",   // 17
    "GetSystemDeployMode",      // 18
    "SetSystemDeployMode",      // 19
    "SetSensorType",            // 20
    "TurnOnDisplay",            // 21
    "TurnOnAutoAdjustDisplay",  // 22
    "GetAlertConfig",           // 23
    "CheckPNTokenEnabled",      // 24
    "SetAlertEnableConfig",     // 25
    "SetSensorAlertConfig",     // 26
    "SetPNToken",               // 27
    "UpdateFirmware",           // 28
    "Restart",                  // 29
    "RestoreFactory",           // 30
    "SetDebugFlag",             // 31
    "GetFanOpMode",             // 32
    "SetFanOpMode"              // 33
};

CmdKey strToCmdKey(const char *str)
{
    size_t len = strlen(str);

    if (!str || len == 0) return DoNothing;

    for (int i = 0; i < CmdKeyMaxValue; ++i) {
        if (strlen(CmdKeyStr[i]) == len && strcmp(CmdKeyStr[i], str) == 0)
            return CmdKey(i);
    }

    return DoNothing;
}

const char* cmdKeyToStr(CmdKey cmdKey)
{
    return CmdKeyStr[cmdKey];
}
