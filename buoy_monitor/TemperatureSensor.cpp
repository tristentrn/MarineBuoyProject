#include "TemperatureSensor.h"

TemperatureSensor::TemperatureSensor(uint8_t pin, uint8_t type)
: _dht(pin, type) {}

void TemperatureSensor::begin() {
  _dht.begin();
}

TemperatureSensorReading TemperatureSensor::read(){
  TemperatureSensorReading r;
  r.tempF = _dht.readTemperature(true);  // Fahrenheit
  r.humidity = _dht.readHumidity();
  
  r.tempValid = !isnan(r.tempF);
  r.humidityValid = !isnan(r.humidity);
  return r;
}
