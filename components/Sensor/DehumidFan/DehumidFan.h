/*
 * DehumidFan class
 * Copyright (c) 2017 Shenghua Su
 *
 */

#ifndef _DEHUMID_FAN_H
#define _DEHUMID_FAN_H

class DehumidFan
{
public:
  void init();

  // --- fan power management
  bool fan1On();
  bool fan2On();
  void setFan1On(bool on = true);
  void setFan2On(bool on = true);
};

#endif // _DEHUMID_FAN_H
