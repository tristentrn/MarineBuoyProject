#include "WifiManager.h"

WifiManager::WifiManager(const char* ssid, const char* pass, uint32_t retryMs)
: _ssid(ssid), _pass(pass), _retryMs(retryMs), _lastRetryMs(0) {}

void WifiManager::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);   // helpful on ESP32
  WiFi.persistent(false);        // avoid writing creds to flash repeatedly
  WiFi.begin(_ssid, _pass);

  Serial.print("Connecting to Wi-Fi");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 20000UL) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Initial Wi-Fi connection failed.");
  }

  _lastRetryMs = millis();
}

void WifiManager::ensureConnected() {
  if (WiFi.status() == WL_CONNECTED) return;

  uint32_t now = millis();
  if ((now - _lastRetryMs) < _retryMs) return;
  _lastRetryMs = now;

  Serial.println("Wi-Fi disconnected. Reconnecting...");

  // Try lightweight reconnect first
  WiFi.reconnect();

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 5000UL) {
    delay(100);
  }

  // If still not connected, do full begin
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(_ssid, _pass);
    t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 8000UL) {
      delay(100);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Reconnected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.printf("Reconnect failed. status=%d\n", (int)WiFi.status());
  }
}

bool WifiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}
