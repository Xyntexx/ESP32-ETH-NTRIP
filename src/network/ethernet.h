#pragma once
#define _ASYNC_WEBSERVER_LOGLEVEL_       3

#include <core/defines.h>
#include <Arduino.h>
#include <WebServer_ESP32_SC_W6100.hpp>

// Function declarations
void EthEvent(WiFiEvent_t event);
bool initializeEthernet(); 