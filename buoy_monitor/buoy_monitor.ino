#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "AppConfig.h"
#include "StatusModel.h"
#include "LedController.h"
#include "WifiManager.h"
#include "WeatherService.h"
#include "BNO055Sensor.h"
#include "TemperatureSensor.h"
#include "Secret.h"

/**
 * @file BuoyProject.ino
 * @brief Main application coordinator.
 *
 * Responsibilities:
 *  1) Manage Wi-Fi connectivity and periodic weather refresh (NWS).
 *  2) Synchronize real clock using NTP in Pacific Time (PST/PDT).
 *  3) Continuously sample BNO055 motion data through BNO055Sensor module.
 *  4) Read DHT temperature/humidity when a motion window result is ready.
 *  5) Update LED state from wave status (currently wave-only policy).
 *  6) Print telemetry every 10 seconds (throttled logging).
 *  7) Upload latest telemetry to Firebase.
 *  8) Append historical logs to Firebase at a lower rate.
 **/

// ---------------- Module instances ----------------
LedController leds(PIN_LED_RED, PIN_LED_YELLOW, PIN_LED_GREEN);
WifiManager wifi(WIFI_SSID, WIFI_PASS, WIFI_RETRY_MS);
WeatherService weather(USER_AGENT, LAT, LON);
BNO055Sensor bnoSensor(BNO_ADDR);
TemperatureSensor tempSensor(DHT_PIN, DHT_TYPE);

// ---------------- Shared state ----------------
WeatherSnapshot ws;
uint32_t lastWeatherMs = 0;
bool motionReady = false;

// Serial/telemetry throttle
static constexpr uint32_t BNO_PRINT_MS = 10000; // 10 sec
uint32_t lastBnoPrintMs = 0;

// Firebase logs rate limit (history append)
static constexpr uint32_t LOG_MS = 30000; // 30 sec
uint32_t lastLogMs = 0;

// NTP reliability state
bool lastWifiConnected = false;
uint32_t lastNtpRetryMs = 0;
static constexpr uint32_t NTP_RETRY_MS = 30000; // 30 sec retry if unsynced

/**
 * @brief Synchronize ESP32 clock using Pacific Time (PST/PDT).
 */
void syncClockWithNTP() {
  // Pacific Time with DST rules
  configTzTime("PST8PDT,M3.2.0/2,M11.1.0/2", "pool.ntp.org", "time.nist.gov");

  time_t nowTs;
  time(&nowTs);

  int tries = 0;
  while (nowTs < 100000 && tries < 20) { // wait up to ~10s
    delay(500);
    time(&nowTs);
    tries++;
  }

  if (nowTs > 100000) {
    Serial.println("NTP time synced (PST/PDT)");
    struct tm ti;
    localtime_r(&nowTs, &ti);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &ti);
    Serial.print("Local time: ");
    Serial.println(buf);
  } else {
    Serial.println("NTP sync not ready");
  }
}

/**
 * @brief Optional helper to fuse statuses conservatively.
 */
RiskStatus fuseStatus(RiskStatus a, RiskStatus b) {
  if (a == RiskStatus::BAD || b == RiskStatus::BAD) return RiskStatus::BAD;
  if (a == RiskStatus::OK  || b == RiskStatus::OK)  return RiskStatus::OK;
  return RiskStatus::GOOD;
}

/**
 * @brief True if clock appears valid (not epoch/1970).
 */
bool isTimeSynced() {
  time_t nowTs;
  time(&nowTs);
  return (nowTs > 1700000000);
}

/**
 * @brief Fill output with local time string or "UNSYNCED".
 */
void getLocalTimeString(char* out, size_t outSize) {
  if (!isTimeSynced()) {
    snprintf(out, outSize, "UNSYNCED");
    return;
  }

  time_t nowTs;
  time(&nowTs);
  struct tm ti;
  localtime_r(&nowTs, &ti);
  strftime(out, outSize, "%Y-%m-%d %H:%M:%S %Z", &ti);
}

/**
 * @brief Build date/time strings used in Firebase payload.
 */
void buildDateTimeStrings(String& outDate, String& outTime) {
  if (!isTimeSynced()) {
    outDate = "UNSYNCED";
    outTime = "UNSYNCED";
    return;
  }

  time_t nowTs;
  time(&nowTs);
  struct tm ti;
  localtime_r(&nowTs, &ti);

  char dateBuf[11]; // YYYY-MM-DD
  char timeBuf[16];  // HH:MM:SS
  strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &ti);
  strftime(timeBuf, sizeof(timeBuf), "%I:%M:%S %p", &ti);  // 12-hour + AM/PM
  if (timeBuf[0] == '0') {
    memmove(timeBuf, timeBuf + 1, strlen(timeBuf)); // shift left 1 char
  }

  outDate = String(dateBuf);
  outTime = String(timeBuf);
}

/**
 * @brief Upload one telemetry snapshot to /buoy/latest.json (overwrite).
 */
bool uploadLatestToFirebase(const String& dateStr,
                            const String& timeStr,
                            float tempF, bool tempValid,
                            float humidity, bool humidityValid,
                            float rms,
                            const WeatherSnapshot & ws,
                            const String& buoyStatus) {
  //Must have Wifi
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure(); // testing only

  HTTPClient https;
  String url = String(FIREBASE_BASE) + "/buoy/latest.json";

  //Start request
  if (!https.begin(client, url)) {
    Serial.println("Firebase latest begin() failed");
    return false;
  }

  https.setTimeout(10000);
  https.addHeader("Content-Type", "application/json");

  StaticJsonDocument<768> doc;

  doc["date"] = dateStr;
  doc["time"] = timeStr;


  if (tempValid) doc["temperatureF"] = tempF; else doc["temperatureF"] = nullptr;
  if (humidityValid) doc["humidity"] = humidity; else doc["humidity"] = nullptr;
  
  //wave metric
  doc["rms"] = rms;

  // Weather fields from NWS
  String forecast = ws.shortForecast;
  forecast.trim();
  if (forecast.length() == 0) forecast = "NWS unavailable";
 // doc["weatherLabel"]    = ws.shortForecast;     // label shown in UI
  doc["weatherForecast"] = ws.shortForecast;     // same string (keep both if you want)
  
  //NWS wind fields
  doc["windMph"]         = ws.windMph;
  doc["gustMph"]         = ws.gustMph;
  doc["windDirection"]   = ws.windDirection;

  // wave condition
  doc["buoyStatus"] = buoyStatus;

  String body;
  serializeJson(doc, body);

  int code = https.PUT(body);
  String resp = https.getString();
  https.end();

  Serial.printf("Firebase latest PUT HTTP %d\n", code);
  if (code < 200 || code >= 300) {
    Serial.println("Firebase latest response:");
    Serial.println(resp);
    return false;
  }

  return true;
}

/**
 * @brief Append one telemetry snapshot to /buoy/logs.json (history).
 */
bool appendLogToFirebase(const String& dateStr,
                         const String& timeStr,
                         float tempF, bool tempValid,
                         float humidity, bool humidityValid,
                         float rms,
                         const WeatherSnapshot& ws,
                         const String& buoyStatus) {
  
  //must have wifi
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure(); // testing only

  HTTPClient https;
  String url = String(FIREBASE_BASE) + "/buoy/logs.json";

  if (!https.begin(client, url)) {
    Serial.println("Firebase logs begin() failed");
    return false;
  }

  https.setTimeout(10000);
  https.addHeader("Content-Type", "application/json");

  StaticJsonDocument<768> doc;

  doc["date"] = dateStr;
  doc["time"] = timeStr;

  // Must match Firebase rules exactly:
if (tempValid)    doc["temperatureF"] = tempF;    else doc["temperatureF"] = nullptr;
if (humidityValid) doc["humidity"]     = humidity; else doc["humidity"]     = nullptr;
  
  doc["rms"] = rms;
  
  // Forecast as label
  String forecast = ws.shortForecast;
  forecast.trim();
  if (forecast.length() == 0) forecast = "NWS unavailable";

  //doc["weatherLabel"]    = ws.shortForecast;
  doc["weatherForecast"] = ws.shortForecast;
  
  //Wind Fields
  doc["windMph"]         = ws.windMph;
  doc["gustMph"]         = ws.gustMph;
  doc["windDirection"]   = ws.windDirection;

  //wave status only
  doc["buoyStatus"] = buoyStatus;

  String body;
  serializeJson(doc, body);

  int code = https.POST(body);
  String resp = https.getString();
  https.end();

  Serial.printf("Firebase logs POST HTTP %d\n", code);
  if (code < 200 || code >= 300) {
    Serial.println("Firebase logs response:");
    Serial.println(resp);
    return false;
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // 1) LED init
  leds.begin();
  leds.set(RiskStatus::OK);

  // 2) Wi-Fi
  wifi.begin();

  Serial.printf("WiFi.status()=%d\n", (int)WiFi.status());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
  }

  lastWifiConnected = wifi.isConnected();

  // 3) Startup NTP sync
  if (lastWifiConnected) {
    syncClockWithNTP();
  } else {
    Serial.println("Skipping startup NTP sync (no Wi-Fi).");
  }

  // 4) Initial weather fetch
  if (wifi.isConnected()) {
    if (weather.refresh(ws)) {
      Serial.print("Initial weather status: ");
      Serial.println(toString(ws.weatherStatus));
    } else {
      Serial.println("Initial weather fetch failed.");
    }
  } else {
    Serial.println("Wi-Fi not connected at boot; weather fetch skipped.");
  }

  // 5) BNO055
  motionReady = bnoSensor.begin();
  if (!motionReady) {
    Serial.println("BNO055 NOT detected");
    // leds.set(RiskStatus::BAD); // optional if IMU is required
  } else {
    Serial.println("BNO055 detected");
  }

  // 6) DHT
  tempSensor.begin();
  Serial.println("DHT ready");

  // 7) Timers
  uint32_t startMs = millis();
  lastWeatherMs = startMs;
  lastBnoPrintMs = startMs;
  lastNtpRetryMs = startMs;
  lastLogMs = startMs;
}

void loop() {
  uint32_t now = millis();

  // Keep Wi-Fi alive
  wifi.ensureConnected();

  // NTP re-sync behavior
  bool wifiNow = wifi.isConnected();

  // Edge detect: disconnected -> connected
  if (wifiNow && !lastWifiConnected) {
    Serial.println("Wi-Fi reconnected, syncing NTP...");
    syncClockWithNTP();
  }

  // Periodic retry if time still unsynced
  if (wifiNow && !isTimeSynced() && (now - lastNtpRetryMs >= NTP_RETRY_MS)) {
    lastNtpRetryMs = now;
    Serial.println("Time unsynced, retrying NTP...");
    syncClockWithNTP();
  }

  lastWifiConnected = wifiNow;

  // Weather refresh
  if (now - lastWeatherMs >= WEATHER_MS) {
    lastWeatherMs = now;

    if (wifi.isConnected()) {
      if (weather.refresh(ws)) {
        Serial.print("Weather status: ");
        Serial.println(toString(ws.weatherStatus));
      } else {
        Serial.println("Weather refresh failed; keeping previous weather state.");
      }
    } else {
      Serial.println("Skipping weather refresh (no Wi-Fi).");
    }
  }

  // Motion sampling / window processing
  if (motionReady) {
    bnoSensor.update();

    if (bnoSensor.hasWindowResult()) {
      BNO055SensorReading m = bnoSensor.takeWindowResult();
      TemperatureSensorReading t = tempSensor.read();

      // LED policy: wave-only for now
      RiskStatus waveStatus = classifyWaveFromRms(m.rms);
      RiskStatus finalStatus = waveStatus;
      // RiskStatus finalStatus = fuseStatus(ws.weatherStatus, waveStatus);
      leds.set(finalStatus);

      // weather label gives actual weather forecase (hourly updated)
      String weatherLabel = ws.shortForecast;
      weatherLabel.trim();
      if (weatherLabel.length() == 0) weatherLabel = "NWS unavailable";

      // Throttled print + upload
      if (now - lastBnoPrintMs >= BNO_PRINT_MS) {
        lastBnoPrintMs = now;

        // Serial print
        char timeBuf[40];
        getLocalTimeString(timeBuf, sizeof(timeBuf));
        Serial.print("Time=");
        Serial.print(timeBuf);

        Serial.print("  TempF=");
        if (t.tempValid) Serial.print(t.tempF, 1); else Serial.print("NaN");

        Serial.print("  Humidity=");
        if (t.humidityValid) Serial.print(t.humidity, 1); else Serial.print("NaN");

        Serial.print("%  RMS=");
        Serial.print(m.rms, 4);

        Serial.print("  Wave=");
        Serial.print(toString(waveStatus));

        Serial.print("  Weather=");
        Serial.print(toString(ws.weatherStatus));

        Serial.print("  WeatherLabel=");
        Serial.print(weatherLabel);

        Serial.print("  FinalLED=");
        Serial.print(toString(finalStatus));

        Serial.print("  PeriodDBG=");
        Serial.print(m.avgPeriod, 2);
        Serial.print("s  Crossings=");
        Serial.println(m.crossings);

        // Build payload fields
        String buoyStatus = String(toString(finalStatus));
        String dateStr, timeStr;
        buildDateTimeStrings(dateStr, timeStr);

        // Always update latest snapshot
        bool upOk = uploadLatestToFirebase(
          dateStr,
          timeStr,
          t.tempF, t.tempValid,
          t.humidity, t.humidityValid,
          m.rms,
          ws,
          buoyStatus
        );
        if (!upOk) {
          Serial.println("WARNING: Firebase latest upload failed.");
        }

        // Append history at lower rate
        if (now - lastLogMs >= LOG_MS) {
          lastLogMs = now;

          bool logOk = appendLogToFirebase(
            dateStr,
            timeStr,
            t.tempF, t.tempValid,
            t.humidity, t.humidityValid,
            m.rms,
            ws,
            buoyStatus
          );
          if (!logOk) {
            Serial.println("WARNING: Firebase log append failed.");
          }
        }
      }
    }
  }
}

