# MarineBuoyProject

MarineBuoyProject is a two-part system for collecting buoy telemetry data and displaying it on a web dashboard.

It includes:

- **`buoy_monitor/`** → Arduino/ESP32 firmware that reads sensors, fetches weather data, and uploads telemetry to Firebase
- **`buoy-website/`** → Web dashboard (HTML/CSS/JS) that reads from Firebase and displays live + historical buoy data

---

## Project Overview

This project is designed to monitor environmental and buoy-related conditions by combining:

- **Embedded hardware + sensors** (ESP32, IMU, temperature/humidity, etc.)
- **Cloud data storage** (Firebase Realtime Database)
- **Web visualization** (JavaScript + Firebase Web SDK)

The goal is to make buoy conditions easy to monitor in real time through a browser dashboard.

---

## Repository Structure

```text
MarineBuoyProject/
├── buoy_monitor/         # Arduino / ESP32 firmware
│   ├── *.ino
│   ├── *.h
│   └── *.cpp
│
├── buoy-website/         # Web dashboard (HTML/CSS/JS)
│   ├── index.html
│   ├── style.css
│   └── script.js
│
├── .gitignore
└── README.md

---

## Authors

- **Tristen Tran**
- **Shreeya Vachaani**