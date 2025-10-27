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
#include "esp_task_wdt.h"

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

    // Initialize watchdog timer
    esp_task_wdt_init(WATCHDOG_TIMEOUT_MS / 1000, true); // Convert ms to seconds, panic on timeout
    esp_task_wdt_add(NULL); // Add current task (loop task) to WDT
    debugf("Watchdog timer initialized (%ds timeout)", WATCHDOG_TIMEOUT_MS / 1000);

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
    // Reset watchdog timer to prevent timeout
    esp_task_wdt_reset();

    // Current time for intervals
    unsigned long currentMillis = millis();

    // Print system uptime periodically
    if (currentMillis - lastUptimePrint >= UPTIME_PRINT_INTERVAL_MS) {
        lastUptimePrint = currentMillis;
        debug("System Uptime: " + getUptimeString());
    }

    delay(1000);
}
