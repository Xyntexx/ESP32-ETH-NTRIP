# NTRIP Server ESP32 Base Station

A NTRIP (Networked Transport of RTCM via Internet Protocol) server implementation for ESP32, designed to serve as a base station for RTK (Real-Time Kinematic) GPS applications.

## Features

- NTRIP v1 server implementation
- Web interface for configuration and monitoring
- Ethernet connectivity with W6100 chip
- UDP logging and system status monitoring
- GNSS (ZED-F9P) module integration with survey-in control

## Hardware Requirements

- ESP32-C3
- W6100 Ethernet module
- Compatible GPS module (u-blox GNSS)
- Power supply

## Software Requirements

- PlatformIO IDE
- Arduino framework
- Required libraries:
  - ArduinoJson (^6.20.0)
  - SparkFun u-blox GNSS Arduino Library (^2.2.20)
  - AsyncWebServer_ESP32_SC_W6100 (^1.8.1)