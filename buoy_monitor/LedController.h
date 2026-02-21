#pragma once
#include "StatusModel.h"

class LedController {
public:
  LedController(int redPin, int yellowPin, int greenPin);
  void begin();
  void set(RiskStatus status);

  void allOn();
  void allOff();

private:
  int _red, _yellow, _green;
};
