#include "BNO055Sensor.h"
#include "AppConfig.h"
#include <math.h>

BNO055Sensor::BNO055Sensor(uint8_t bnoAddr)
: _bno(55, bnoAddr) {}

bool BNO055Sensor::begin() {
  Wire.begin();

  if (!_bno.begin()) {
    _ready = false;
    return false;
  }

  _bno.setExtCrystalUse(false);
  _ready = true;
  _lastSampleMs = millis();
  return true;
}

void BNO055Sensor::update() {
  if (!_ready) return;

  unsigned long now = millis();
  if (now - _lastSampleMs < SAMPLE_DT_MS) return;
  _lastSampleMs = now;

  // Read accel + gravity
  imu::Vector<3> accel = _bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
  imu::Vector<3> grav  = _bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);

  // Normalize gravity vector
  float gm = sqrt(grav.x() * grav.x() + grav.y() * grav.y() + grav.z() * grav.z());
  if (gm < 0.1f) gm = 1.0f;
  float gx = grav.x() / gm;
  float gy = grav.y() / gm;
  float gz = grav.z() / gm;

  // Vertical acceleration relative to gravity axis
  float aAlongG = accel.x() * gx + accel.y() * gy + accel.z() * gz;
  float aVert = aAlongG - 9.81f;

  // Low-pass filter
  _aLP = (1.0f - BNO_ALPHA_LP) * _aLP + BNO_ALPHA_LP * aVert;

  // RMS accumulators
  _sumSquares += _aLP * _aLP;
  _sampleCount++;

  // Zero-crossing period debug
  if (_prev < 0.0f && _aLP >= 0.0f) {
    if (_lastCrossMs != 0) {
      float T = (now - _lastCrossMs) / 1000.0f;
      if (T >= 0.3f && T <= 10.0f) {
        _periodSum += T;
        _periodCount++;
      }
    }
    _lastCrossMs = now;
  }
  _prev = _aLP;

  // Window complete
  if (_sampleCount >= WINDOW_SAMPLES) {
    _latest.rms = sqrt(_sumSquares / (float)_sampleCount);
    _latest.avgPeriod = (_periodCount > 0) ? (_periodSum / _periodCount) : 0.0f;
    _latest.crossings = _periodCount;
    _latest.valid = true;
    _hasResult = true;

    // Reset window
    _sumSquares = 0.0f;
    _sampleCount = 0;
    _periodSum = 0.0f;
    _periodCount = 0;
  }
}

bool BNO055Sensor::hasWindowResult() const {
  return _hasResult;
}

BNO055SensorReading BNO055Sensor::takeWindowResult() {
  _hasResult = false;
  return _latest;
}
