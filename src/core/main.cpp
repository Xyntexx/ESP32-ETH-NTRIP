#include "defines.h"
#include <Arduino.h>
#include "network/web_server.h"
#include "network/ethernet.h"
#include "utils/udp_stream.h"

#include "utils/log.h"
#include "hardware/gps.h"
#include "utils/settings.h"
#include "network/ntrip.h"
#include "utils/system_status.h"
#include "WebServer_ESP32_SC_W6100.h"

constexpr unsigned long UPTIME_PRINT_INTERVAL = 300000; // 5 minutes
unsigned long lastUptimePrint = 0;

// Create UDP stream instance
UDPStream udpStream;

bool initialize_systems() {
    // Read settings
    readSettings();
    debug("Settings loaded");

    // Initialize Ethernet
    initializeEthernet();
    debug("Ethernet initialized");

    // Initialize UDP logging after Ethernet is up
    initUDPLogging();

    // Initialize WebServer
    initializeWebServer();
    debug("Web server initialized");

    // Initialize GPS
    if (!initializeGPS()) {
        error("Failed to initialize GPS");
        return false;
    }
    debug("GPS initialized");
    ntrip_handle_init();
    debug("NTRIP initialized");
    return true;
}

void setup()
{
    startupTime = millis();  // Record startup time

    // Initialize basic logging first
    initLogging();

    // Log system startup
    info("System Startup - Version " + String(FIRMWARE_VERSION) + " (" + String(BUILD_DATE) + ")");

    if (!initialize_systems())
    {
        error("System initialization failed");
    }else
    {
        info("System initialization successful");
    }

}

void loop()
{
    // Current time for intervals
    unsigned long currentMillis = millis();

    // Print system uptime every 60 seconds
    if (currentMillis - lastUptimePrint >= UPTIME_PRINT_INTERVAL) {
        lastUptimePrint = currentMillis;
        debug("System Uptime: " + getUptimeString());
    }

    delay(1000);
}
