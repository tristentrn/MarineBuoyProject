#pragma once
#include <Arduino.h>
#include <WiFi.h>

class WifiManager {
public:
  WifiManager(const char* ssid, const char* pass, uint32_t retryMs);
  void begin();
  void ensureConnected();
  bool isConnected() const;

private:
  const char* _ssid;
  const char* _pass;
  uint32_t _retryMs;
  uint32_t _lastRetryMs;
};
