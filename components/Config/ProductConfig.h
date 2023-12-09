/*
 * ProductConfig.h product config
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _PRODUCT_CONF_H_INCLUDED
#define _PRODUCT_CONF_H_INCLUDED

#include "SensorConfig.h"

#ifndef PRODUCT_TYPE

  #define ONLY_PM_TYPE       0
  #define PM_HCHO_TYPE       1
  #define PM_HCHO_CO2_TYPE   2

  #define PRODUCT_TYPE PM_HCHO_TYPE

#endif

#if   PRODUCT_TYPE == ONLY_PM_TYPE
  #define PRODUCT_PM_SENSOR  PMS5003
  #define PRODUCT_CO2_SENSOR NoneSensor
#elif PRODUCT_TYPE == PM_HCHO_TYPE
  #define PRODUCT_PM_SENSOR  PMS5003ST
  #define PRODUCT_CO2_SENSOR NoneSensor
#elif PRODUCT_TYPE == PM_HCHO_CO2_TYPE
  #define PRODUCT_PM_SENSOR  PMS5003ST
  #define PRODUCT_CO2_SENSOR DSCO220
#endif

#endif // _PRODUCT_CONF_H_INCLUDED
