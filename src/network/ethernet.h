#pragma once

#include <Arduino.h>
#include <WebServer_ESP32_SC_W6100.hpp>

// Function declarations
void EthEvent(WiFiEvent_t event);
bool initializeEthernet(); 