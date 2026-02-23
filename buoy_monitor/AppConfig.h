#pragma once
#include <Arduino.h>
#include <DHT.h>
#include "Secret.h"

/**
 * @file Config.h
 * @brief Central place for project-wide constants and tunable settings.
 *
 * Keep all "magic numbers" here so you can change behavior without touching logic files.
 */


// ---------------- NWS API ----------------
static const char* USER_AGENT = "UCI-Buoy-Demo (contact: jhuefner@uci.edu)";
static constexpr float LAT = 33.5453f;
static constexpr float LON = -117.7814f;

// ---------------- LED pins ----------------
static constexpr int PIN_LED_RED    = 12;
static constexpr int PIN_LED_YELLOW = 9;
static constexpr int PIN_LED_GREEN  = 6;

// ---------------- Timing ----------------
static constexpr uint32_t WEATHER_MS = 15UL * 60UL * 1000UL;
static constexpr uint32_t WIFI_RETRY_MS = 10000UL;

// ---------------- BNO055 ----------------
static constexpr uint8_t BNO_ADDR = 0x29;

// ---------------- DHT ----------------
static constexpr uint8_t DHT_PIN  = 3;
static constexpr uint8_t DHT_TYPE = DHT11;

// ---------------- Motion sampling ----------------
static constexpr int BNO_SAMPLE_RATE = 50;
static constexpr int WINDOW_MS = 2000;
static constexpr int WINDOW_SAMPLES = (BNO_SAMPLE_RATE * WINDOW_MS) / 1000;
static constexpr uint32_t SAMPLE_DT_MS = 1000UL / BNO_SAMPLE_RATE;
static constexpr float BNO_ALPHA_LP = 0.25f;

// ---------------- Wave thresholds ----------------
static constexpr float RMS_BAD_MAX = 0.8f;
static constexpr float RMS_OK_MAX  = 2.0f;
