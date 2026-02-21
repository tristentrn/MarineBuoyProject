#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

struct BNO055SensorReading {
  float rms = 0.0f;
  float avgPeriod = 0.0f;
  int crossings = 0;
  bool valid = false;
};

class BNO055Sensor {
public:
  explicit BNO055Sensor (uint8_t bnoAddr = 0x29);

  bool begin();
  void update();                       // call every loop iteration
  bool hasWindowResult() const;        // true when window is ready
  BNO055SensorReading takeWindowResult();    // consume latest result

private:
  Adafruit_BNO055 _bno;
  bool _ready = false;

  /*// Sampling/window config
  static constexpr int sampleRate = 50;
  static constexpr int windowMs = 2000;
  static constexpr int windowSamples = (sampleRate * windowMs) / 1000;*/

  // Filter + accumulators
  float _aLP = 0.0f;
  //static constexpr float alphaLP = 0.25f;
  float _sumSquares = 0.0f;
  int _sampleCount = 0;

  // Zero-crossing debug
  float _prev = 0.0f;
  unsigned long _lastCrossMs = 0;
  float _periodSum = 0.0f;
  int _periodCount = 0;

  // Timing
  unsigned long _lastSampleMs = 0;
  //static constexpr uint32_t sampleDtMs = 1000UL / sampleRate;

  bool _hasResult = false;
  BNO055SensorReading _latest{};
};
