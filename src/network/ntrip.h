#pragma once

#include <Arduino.h>
#include "hardware/gps.h"
#include "WebServer_ESP32_SC_W6100.hpp"

// Structure to track NTRIP connection status
struct NTRIPStatus {
    bool connected;
    uint32_t bytesSent;
    String lastError;
    int reconnectAttempts;
    unsigned long connectionOpenedAt;
    int protocolVersion;  // Added to track protocol version per connection
};

void ntrip_handle_init();

// Function declarations
bool stopNTRIP();
bool startNTRIP();
bool stopNTRIP(WiFiClient &client, bool isPrimary);

// External status variables
extern NTRIPStatus NtripPrimaryStatus;
extern NTRIPStatus NtripSecondaryStatus;

extern  size_t write_count;