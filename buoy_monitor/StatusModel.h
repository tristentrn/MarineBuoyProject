#pragma once
#include "AppConfig.h"
#include <Arduino.h>

/**
 * @brief Unified status levels used across modules.
 */

enum class RiskStatus : uint8_t { GOOD, OK, BAD };

/**
 * @brief Convert RiskStatus enum to human-readable string.
 */

inline const char* toString(RiskStatus s) {
  switch (s) {
    case RiskStatus::GOOD: return "GOOD";
    case RiskStatus::OK:   return "OK";
    case RiskStatus::BAD:  return "BAD";
    default: return "OK";
  }
}

inline RiskStatus classifyWaveFromRms(float rms) {
  if (rms < RMS_BAD_MAX) return RiskStatus::BAD;
  if (rms < RMS_OK_MAX)  return RiskStatus::OK;
  return RiskStatus::GOOD;
}

/**
 * @brief Weather data snapshot after parsing one forecast period.
 */

struct WeatherSnapshot {
  int windMph = -1;       // Parsed wind speed (mph), -1 if unavailable
  int gustMph = -1;       // Parsed gust speed (mph), -1 if unavailable
  String shortForecast;   // NWS shortForecast text
  String windDirection;
  RiskStatus weatherStatus = RiskStatus::OK;   // Derived status classification
};

