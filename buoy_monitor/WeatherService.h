#pragma once
#include <Arduino.h>
#include "StatusModel.h"

class WeatherService {
public:
  WeatherService(const char* userAgent, float lat, float lon);
  bool refresh(WeatherSnapshot& out);

private:
  const char* _userAgent;
  float _lat, _lon;
  String _hourlyUrlCached;

  String httpGET(const String& url, const char* acceptHeader = "application/json");
  String ensureUnitsUS(const String& url) const;

  int parseWindMph(const char* windStr) const;
  bool containsAny(const String& sLower, const char* words[], int n) const;
  RiskStatus classifyWeather(int wind, int gust, const String& fcLower) const;

  bool fetchPointsAndCacheHourly();

  // Stream parse just period[0]
  bool fetchHourlyPeriod0Streamed(const String& hourlyUrl, WeatherSnapshot& out);
};
