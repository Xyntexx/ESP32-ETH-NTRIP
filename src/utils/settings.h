#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

extern DynamicJsonDocument settings;
extern DynamicJsonDocument status;

bool readSettings();
bool writeSettings(String name, String value);
bool writeSettings(String name, double value);
bool writeSettings(String name, int64_t value);