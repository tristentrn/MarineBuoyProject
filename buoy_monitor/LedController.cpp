#include <Arduino.h>
#include "LedController.h"

LedController::LedController(int redPin, int yellowPin, int greenPin)
: _red(redPin), _yellow(yellowPin), _green(greenPin) {}

void LedController::begin() {
  pinMode(_red, OUTPUT);
  pinMode(_yellow, OUTPUT);
  pinMode(_green, OUTPUT);
  allOff();
}

void LedController::allOff() {
  digitalWrite(_red, LOW);
  digitalWrite(_yellow, LOW);
  digitalWrite(_green, LOW);
}

void LedController::set(RiskStatus status) {
  allOff();
  if (status == RiskStatus::GOOD) {
    digitalWrite(_green, HIGH);
  } else if (status == RiskStatus::OK) {
    digitalWrite(_yellow, HIGH);
  } else {
    digitalWrite(_red, HIGH);
  }
}
