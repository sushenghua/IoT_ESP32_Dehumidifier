/*
 * SensorConfig
 * Copyright (c) 2017 Shenghua Su
 *
 */

#include "SensorConfig.h"

// static const char *SensorDataTypeStr = { "PM", "HCHO", "CO2", "TEMP", "HUMID" };

static const char * const SensorTypeStr[] = {
  "NoneSensor",   // 0
  "PMS5003",      // 1
  "PMS5003T",     // 2
  "PMS5003S",     // 3
  "PMS5003ST",    // 4
  "DSCO220",      //= 5,
  "MPU6050"       //= 6,
};

const char * sensorTypeStr(SensorType type)
{
  return SensorTypeStr[type];
}

uint32_t capabilityForSensorType(SensorType type)
{
  switch(type) {
    case PMS5003:
      return PM_CAPABILITY_MASK;

    case PMS5003T:
      return (PM_CAPABILITY_MASK | TEMP_HUMID_CAPABILITY_MASK);

    case PMS5003S:
      return (PM_CAPABILITY_MASK | HCHO_CAPABILITY_MASK);

    case PMS5003ST:
      return (PM_CAPABILITY_MASK | HCHO_CAPABILITY_MASK | TEMP_HUMID_CAPABILITY_MASK);

    case DSCO220:
      return CO2_CAPABILITY_MASK;

    case MPU6050:
      return ORIENTATION_CAPABILITY_MASK;

    default:
      return 0;
  }
}

uint16_t rxProtocolLengthForSensorType(SensorType type)
{
  switch(type) {
    case PMS5003:
    case PMS5003T:
    case PMS5003S:
      return 32;

    case PMS5003ST:
      return 40;

    case DSCO220:
      return 12;

    default:
      return 0;
  }
}

uint16_t tempHumidDataPosForSensorType(SensorType type)
{
  switch(type) {
    case PMS5003T:
      return 10;

    case PMS5003ST:
      return 13;

    default:
      return 0xFFFF; // invalid
  }
}

static const char * const SensorDataTypeStr[] = {
  "PM",      // 0
  "HCHO",    // 1
  "CO2",     // 2
  "TEMP",    // 3
  "HUMID",   // 4
  "LUMI",    // 5
};

const char * sensorDataTypeStr(SensorDataType type)
{
  return SensorDataTypeStr[type];
}

static const char * const SensorDataValueStrFormat[] = {
  "%s\"PM\":%.0f",    // 0
  "%s\"HCHO\":%.2f",  // 1
  "%s\"CO2\":%.0f",   // 2
  "%s\"TEMP\":%.1f",  // 3
  "%s\"HUMID\":%.1f", // 4
  "%s\"LUMI\":%.0f"   // 5
};

const char * sensorDataValueStrFormat(SensorDataType type)
{
  return SensorDataValueStrFormat[type];
}

static SensorAlertMask const SENSOR_ALERT_MASKS[6] = {
  PM_ALERT_MASK, HCHO_ALERT_MASK, CO2_ALERT_MASK, TEMP_ALERT_MASK, HUMID_ALERT_MASK, LUMI_ALERT_MASK
};

SensorAlertMask sensorAlertMask(SensorDataType type)
{
  return SENSOR_ALERT_MASKS[type];
}
