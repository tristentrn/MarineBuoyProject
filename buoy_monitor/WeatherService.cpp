#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "WeatherService.h"

/*
  WeatherService.cpp (UPDATED - robust extraction)

  Why this version works:
  - The NWS hourly endpoint is ~160KB.
  - Building that into a String can corrupt / fragment memory.
  - ArduinoJson Filter() from stream can sometimes keep "null" on certain streams.
  - So we do this instead:
      1) Stream-scan for "periods"
      2) Extract ONLY the first object { ... } of periods[0]
      3) Parse that small JSON object with ArduinoJson normally
*/

// ---------- Small helper: ensure URL has units=us ----------
String WeatherService::ensureUnitsUS(const String& url) const {
  if (url.indexOf("units=") >= 0) return url;
  if (url.indexOf('?') >= 0) return url + "&units=us";
  return url + "?units=us";
}

// ---------- Constructor ----------
WeatherService::WeatherService(const char* userAgent, float lat, float lon)
: _userAgent(userAgent), _lat(lat), _lon(lon) {
  _hourlyUrlCached.reserve(180);
}

/*
  httpGET: used only for SMALL endpoints (like /points).
  We DO NOT use this for the giant hourly forecast.
*/
String WeatherService::httpGET(const String& url, const char* acceptHeader) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.setTimeout(15000);
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!https.begin(client, url)) {
    Serial.println("ERROR: https.begin failed");
    return "";
  }

  https.addHeader("User-Agent", _userAgent);
  https.addHeader("Accept", acceptHeader ? acceptHeader : "application/geo+json");
  https.addHeader("Accept-Encoding", "identity"); // avoid gzip

  int code = https.GET();
  String body = https.getString();

  Serial.printf("HTTP %d for %s\n", code, url.c_str());
  Serial.printf("Body length: %u\n", (unsigned)body.length());
  Serial.print("Body snippet: ");
  Serial.println(body.substring(0, 140));

  if (code != HTTP_CODE_OK) {
    Serial.println("Non-200 response:");
    Serial.println(body.substring(0, 400));
    https.end();
    return "";
  }

  if (body.length() == 0) {
    Serial.println("ERROR: 200 response but empty body");
    https.end();
    return "";
  }

  https.end();
  return body;
}

// ---------- Parse mph from strings like "5 mph" or "10 to 15 mph" ----------
int WeatherService::parseWindMph(const char* windStr) const {
  if (!windStr) return -1;

  // Copy into a String for easier normalization
  String s = String(windStr);
  s.trim();
  if (s.length() == 0) return -1;

  s.toLowerCase();

  // Common cases we want to ignore
  if (s == "calm") return 0;

  // NWS sometimes uses "mph", sometimes might include other text.
  // We'll extract the FIRST integer we see.
  int val = -1;
  bool found = false;

  for (int i = 0; i < (int)s.length(); i++) {
    char c = s[i];
    if (c >= '0' && c <= '9') {
      found = true;
      val = 0;
      while (i < (int)s.length() && s[i] >= '0' && s[i] <= '9') {
        val = val * 10 + (s[i] - '0');
        i++;
      }
      break;
    }
  }

  return found ? val : -1;
}

bool WeatherService::containsAny(const String& sLower, const char* words[], int n) const {
  for (int i = 0; i < n; i++) {
    if (sLower.indexOf(words[i]) >= 0) return true;
  }
  return false;
}

RiskStatus WeatherService::classifyWeather(int wind, int gust, const String& fcLower) const {
  const char* badWords[] = {"thunder", "storm", "tstm"};
  const char* okWords[]  = {"rain", "showers", "drizzle", "fog", "mist"};

  if (containsAny(fcLower, badWords, 3)) return RiskStatus::BAD;
  if (gust >= 25 || wind >= 20) return RiskStatus::BAD;

  if (containsAny(fcLower, okWords, 5)) return RiskStatus::OK;
  if (wind >= 12) return RiskStatus::OK;

  return RiskStatus::GOOD;
}

/*
  Fetch /points and cache the forecastHourly URL (small JSON).
*/
bool WeatherService::fetchPointsAndCacheHourly() {
  String pointsUrl = "https://api.weather.gov/points/" + String(_lat, 4) + "," + String(_lon, 4);
  String body = httpGET(pointsUrl, "application/geo+json");
  if (body.length() == 0) return false;

  StaticJsonDocument<16384> doc;
  auto err = deserializeJson(doc, body);
  if (err) {
    Serial.print("ERROR: /points parse: ");
    Serial.println(err.c_str());
    Serial.println(body.substring(0, 400));
    return false;
  }

  const char* hourlyUrl = doc["properties"]["forecastHourly"];
  if (!hourlyUrl || hourlyUrl[0] == '\0') {
    Serial.println("ERROR: /points missing properties.forecastHourly");
    Serial.println(body.substring(0, 400));
    return false;
  }

  _hourlyUrlCached = ensureUnitsUS(String(hourlyUrl));
  Serial.print("Cached hourly URL: ");
  Serial.println(_hourlyUrlCached);
  return true;
}

/*
  STREAM EXTRACTOR:
  - Reads the HTTP body stream and finds the first object in:
      "periods": [ { ... }, ... ]
  - Returns that object as a small JSON string "{...}" in outObj.
  - Handles braces inside strings correctly.
*/
static bool extractFirstPeriodsObject(Stream& s, String& outObj) {
  outObj = "";
  outObj.reserve(4096);

  const char* key = "\"periods\"";
  int match = 0;

  bool inString = false;
  bool escape = false;

  const uint32_t TIMEOUT_MS = 8000;
  uint32_t t0 = millis();

  auto timedOut = [&]() -> bool {
    return (millis() - t0) > TIMEOUT_MS;
  };

  auto readByte = [&]() -> int {
    // Wait for data up to timeout
    while (!s.available()) {
      if (timedOut()) return -1;
      delay(1);
    }
    t0 = millis(); // reset timeout when data arrives
    return s.read();
  };

  // 1) Find the key "periods"
  while (!timedOut()) {
    int c = readByte();
    if (c < 0) break;

    if ((char)c == key[match]) {
      match++;
      if (key[match] == '\0') break; // found
    } else {
      match = ((char)c == key[0]) ? 1 : 0;
    }
  }
  if (key[match] != '\0') return false; // never found "periods"

  // 2) Find the first '{' (start of periods[0] object)
  while (!timedOut()) {
    int c = readByte();
    if (c < 0) break;

    if ((char)c == '{') {
      outObj += '{';
      int depth = 1;

      // 3) Capture until matching closing brace
      while (!timedOut()) {
        int d = readByte();
        if (d < 0) break;

        char ch = (char)d;
        outObj += ch;

        if (escape) {
          escape = false;
          continue;
        }
        if (ch == '\\') {
          if (inString) escape = true;
          continue;
        }
        if (ch == '"') {
          inString = !inString;
          continue;
        }

        if (!inString) {
          if (ch == '{') depth++;
          else if (ch == '}') {
            depth--;
            if (depth == 0) return true; // done
          }
        }
      }
      return false; // timed out mid-object
    }
  }

  return false;
}

/*
  Huge hourly forecast fetch:
  - Do NOT read full body into a String.
  - Stream scan -> extract periods[0] object -> parse small object.
*/
bool WeatherService::fetchHourlyPeriod0Streamed(const String& hourlyUrl, WeatherSnapshot& out) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.setTimeout(20000);
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!https.begin(client, hourlyUrl)) {
    Serial.println("ERROR: hourly https.begin failed");
    return false;
  }

  https.addHeader("User-Agent", _userAgent);
  https.addHeader("Accept", "application/geo+json");
  https.addHeader("Accept-Encoding", "identity");

  int code = https.GET();
  Serial.printf("HTTP %d for %s\n", code, hourlyUrl.c_str());

  if (code != HTTP_CODE_OK) {
    String errBody = https.getString();
    Serial.println("Hourly non-200 body (first 300):");
    Serial.println(errBody.substring(0, 300));
    https.end();
    return false;
  }

  // Extract the first periods object from the stream
  WiFiClient* stream = https.getStreamPtr();
  String p0Json;
  bool ok = extractFirstPeriodsObject(*stream, p0Json);
  https.end();

  if (!ok) {
    Serial.println("ERROR: couldn't extract periods[0] object from stream");
    return false;
  }

  // Now parse the small extracted object
  StaticJsonDocument<4096> p0Doc;
  auto err = deserializeJson(p0Doc, p0Json);
  if (err) {
    Serial.print("ERROR: periods[0] object parse failed: ");
    Serial.println(err.c_str());
    Serial.println("Extracted object (first 400):");
    Serial.println(p0Json.substring(0, 400));
    return false;
  }

  const char* fc = p0Doc["shortForecast"] | "";
  const char* ws = p0Doc["windSpeed"] | "";
  const char* wg = p0Doc["windGust"]  | "";
  const char* wd = p0Doc["windDirection"] | "";

  out.shortForecast = String(fc);
  out.windMph = parseWindMph(ws);
  out.gustMph = parseWindMph(wg);

  if (out.gustMph < 0) out.gustMph = out.windMph;

  out.windDirection = String(wd);   // make sure WeatherSnapshot includes this

  String lower = out.shortForecast;
  lower.toLowerCase();
  out.weatherStatus = classifyWeather(out.windMph, out.gustMph, lower);

  Serial.printf("NWS(extract): fc=\"%s\", wind=%d, gust=%d, dir=%s => %s\n",
                out.shortForecast.c_str(),
                out.windMph,
                out.gustMph,
                out.windDirection.c_str(),
                toString(out.weatherStatus));

  return true;
}

/*
  Public API:
  - Use cached hourly URL if present
  - If hourly fails, refresh /points once and retry
*/
bool WeatherService::refresh(WeatherSnapshot& out) {
  if (_hourlyUrlCached.length() == 0 && !fetchPointsAndCacheHourly()) {
    return false;
  }

  for (int attempt = 0; attempt < 2; attempt++) {
    if (fetchHourlyPeriod0Streamed(_hourlyUrlCached, out)) {
      return true;
    }

    if (attempt == 0) {
      _hourlyUrlCached = "";
      if (!fetchPointsAndCacheHourly()) return false;
    }
  }
  return false;
}