#pragma once
#include <Arduino.h>
#include <DHT.h>

struct TemperatureSensorReading  {
  float tempF = NAN;
  float humidity = NAN;
  bool tempValid = false;
  bool humidityValid = false;
};

class TemperatureSensor {
public:
  TemperatureSensor(uint8_t pin, uint8_t type);

  void begin();
  TemperatureSensorReading read();

private:
  DHT _dht;
};
