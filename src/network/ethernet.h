#pragma once
#define _ASYNC_WEBSERVER_LOGLEVEL_       3

#include <core/defines.h>
#include <Arduino.h>
#include <AsyncWebServer_ESP32_SC_W6100.h>

// MAC addresses configuration
#define NUMBER_OF_MAC 20
extern const byte mac[][NUMBER_OF_MAC];

// Function declarations
void EthEvent(WiFiEvent_t event);
bool initializeEthernet(); 