#pragma once
#define _ASYNC_WEBSERVER_LOGLEVEL_       3

#include <core/defines.h>
#include <Arduino.h>
#include <WebServer_ESP32_SC_W6100.hpp>

// MAC addresses configuration
#define NUMBER_OF_MAC 20
extern const byte mac[][NUMBER_OF_MAC];

// Function declarations
void EthEvent(WiFiEvent_t event);
bool initializeEthernet(); 