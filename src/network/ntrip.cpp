#include <core/defines.h>
#include <Arduino.h>
#include "ntrip.h"
#include "hardware/gps.h"
#include <AsyncWebServer_ESP32_SC_W6100.h>
#include "utils/log.h"


// Primary connection
WiFiClient client;
// Secondary connection
WiFiClient client2;

// Configuration
constexpr int connectionTimeout      = 2000;        // MS threshold for timeout when connecting
constexpr int maxTimeBeforeHangup_ms = 10000; // Disconnect after 10s without data
constexpr int reconnectDelay         = 5000;          // Delay between reconnection attempts
constexpr int slowReconnectDelay     = 30000;        // Slower delay after 5 failed attempts
constexpr int maxReconnectAttempts   = 5;            // Maximum number of attempts before using slow delay
constexpr int rtcmCheckInterval_ms   = 1000;    // Check for RTCM data every second
constexpr int connectionStabilityTimeout_ms = 5000;  // Time to wait before considering connection stable

// Status tracking
NTRIPStatus NtripPrimaryStatus   = {false, 0,  "", 0, 0};
NTRIPStatus NtripSecondaryStatus = {false, 0,  "", 0, 0};
unsigned long lastReport_ms      = 0;
unsigned long lastRtcmCheck_ms   = 0;
unsigned long lastRtcmData_ms    = 0;  // Track when we last received RTCM data
bool ntrip_should_be_connected   = false;


[[noreturn]] void NTRIPTask(void *pvParameter);

bool checkAndConnect(WiFiClient& client, NTRIPStatus& status, bool isPrimary);

enum class NTRIPError {
    NONE,
    CONNECTION_FAILED,
    TIMEOUT,
    INVALID_RESPONSE,
    INVALID_CONFIG,
    AUTH_FAILED,
    RTCM_TIMEOUT,
    SURVEY_IN_ACTIVE
};

String getErrorMessage(NTRIPError error) {
    switch(error) {
        case NTRIPError::CONNECTION_FAILED:
            return "Failed to connect to host";
        case NTRIPError::TIMEOUT:
            return "Connection timeout";
        case NTRIPError::INVALID_RESPONSE:
            return "Invalid server response";
        case NTRIPError::INVALID_CONFIG:
            return "Invalid configuration";
        case NTRIPError::AUTH_FAILED:
            return "Authentication failed";
        case NTRIPError::RTCM_TIMEOUT:
            return "RTCM data timeout";
        default:
            return "Unknown error";
    }
}

bool verifyServerResponse(WiFiClient& client);

void handleError(bool isPrimary, NTRIPError error) {
    String prefix = isPrimary ? "Primary" : "Secondary";
    String errorMsg = getErrorMessage(error);

    errorf("NTRIP %s - %s", prefix.c_str(), errorMsg.c_str());

    if (isPrimary) {
        NtripPrimaryStatus.lastError = errorMsg;
        NtripPrimaryStatus.reconnectAttempts++;
    } else {
        NtripSecondaryStatus.lastError = errorMsg;
        NtripSecondaryStatus.reconnectAttempts++;
    }
}

bool stopNTRIP(WiFiClient& client, bool isPrimary)
{
    info(isPrimary ? "Disconnecting primary NTRIP..." : "Disconnecting secondary NTRIP...");

    client.stop();
    infof("NTRIP %s - Disconnected", isPrimary ? "Primary" : "Secondary");
    if (isPrimary) {
        NtripPrimaryStatus = {false, 0,  "", 0, 0};
    } else {
        NtripSecondaryStatus = {false, 0,  "", 0, 0};
    }
    return true;
}


// Handle all RTCM Checking related things. If return is true, the NTRIP connection should be opened or continued. if false, the NTRIP connection should be closed immediately.
NTRIPError RTCMCheck() {
    const unsigned long currentMillis = millis();

    // If RTCM checks are disabled, always allow connection
    if (!settings["rtcmChk"].as<bool>()) {
        return NTRIPError::NONE;
    }

    // Don't allow connection during survey mode
    if (currentGPSStatus.surveyInActive) {
        return NTRIPError::SURVEY_IN_ACTIVE;
    }

    // If we haven't received RTCM data in the timeout period, don't allow connection
    if (currentMillis - lastRtcmData_ms > maxTimeBeforeHangup_ms) {
        return NTRIPError::RTCM_TIMEOUT;
    }

    // If we have recent RTCM data, allow connection
    return NTRIPError::NONE;
}

// Replace the checkConnectionHealth function with this improved version
NTRIPError checkConnectionHealth(WiFiClient& client, NTRIPStatus& status, bool isPrimary) {
    // If we're in the stability period, don't check too aggressively
    if (millis() - status.connectionOpenedAt < connectionStabilityTimeout_ms) {
        return NTRIPError::NONE;
    }

    // Basic connection check
    if (!client.connected()) {
        handleError(isPrimary, NTRIPError::CONNECTION_FAILED);
        stopNTRIP(client, isPrimary);
        return NTRIPError::CONNECTION_FAILED;
    }

    return NTRIPError::NONE;
}

void handleNTRIP()
{
    static unsigned long previousConnectAttempt = 0;
    static int connectAttemptCount = 0;  // Track number of failed attempts
    const unsigned long currentMillis = millis();

    // Calculate the appropriate delay based on failed attempts
    unsigned long connectInterval = (connectAttemptCount >= maxReconnectAttempts) ? slowReconnectDelay : reconnectDelay;

    // Check if we should be connected based on RTCM data
    NTRIPError error = RTCMCheck();

    // Monitor existing connections
    if (NtripPrimaryStatus.connected && error == NTRIPError::NONE){
        error = checkConnectionHealth(client, NtripPrimaryStatus, true);
    }
    
    if (NtripSecondaryStatus.connected && error == NTRIPError::NONE){
        error = checkConnectionHealth(client2, NtripSecondaryStatus, false);
    }

    // Try to connect if either:
    // 1. This is the first attempt (previousConnectAttempt is 0)
    // 2. The interval has passed
    // and we should be connected
    if ((previousConnectAttempt == 0 ||
        (currentMillis - previousConnectAttempt >= connectInterval)) && error == NTRIPError::NONE) {
        previousConnectAttempt = currentMillis;
        if (checkAndConnect(client, NtripPrimaryStatus, true)) {
            connectAttemptCount = 0;  // Reset counter on successful connection
        } else {
            connectAttemptCount++;  // Increment counter on failed connection
        }
        if (checkAndConnect(client2, NtripSecondaryStatus, false)) {
            connectAttemptCount = 0;  // Reset counter on successful connection
        } else {
            connectAttemptCount++;  // Increment counter on failed connection
        }
    }

    // Check existing connections
    if (NtripPrimaryStatus.connected && error != NTRIPError::NONE) {
        handleError(true, error);
        stopNTRIP(client, true);
    }

    if (NtripSecondaryStatus.connected && error != NTRIPError::NONE) {
        handleError(false, error);
        stopNTRIP(client2, false);
    }

    // Report statistics every 10 seconds
    if (currentMillis - lastReport_ms > 10000) {
        lastReport_ms = currentMillis;
        debugf("NTRIP Status - Primary: %s (%d bytes), Secondary: %s (%d bytes)",
            NtripPrimaryStatus.connected ? "Connected" : "Disconnected",
            NtripPrimaryStatus.bytesSent,
            NtripSecondaryStatus.connected ? "Connected" : "Disconnected",
            NtripSecondaryStatus.bytesSent);
    }
}

bool checkAndConnect(WiFiClient& client, NTRIPStatus& status, const bool isPrimary) {
    const bool isEnabled = isPrimary ?
        settings["enableCaster1"].as<bool>() :
        settings["enableCaster2"].as<bool>();

    if (!isEnabled && status.connected) {
        stopNTRIP(client, isPrimary);
        return false;
    }

    if (!isEnabled) {
        return false;
    }

    // Check if we should be connected based on RTCM data
    if (RTCMCheck() != NTRIPError::NONE){
        return false; // Don't even log, just silently wait for RTCM data
    }

    // If we get here, either RTCM checks are disabled or we have RTCM data
    if (!client.connected() && !status.connected) {
        const char* host = isPrimary ? settings["casterHost1"] : settings["casterHost2"];
        uint16_t port = isPrimary ? settings["casterPort1"].as<uint16_t>() : settings["casterPort2"].as<uint16_t>();
        const char* pw = isPrimary ? settings["rtk_mntpnt_pw1"] : settings["rtk_mntpnt_pw2"];
        const char* mnt = isPrimary ? settings["rtk_mntpnt1"] : settings["rtk_mntpnt2"];

        if (client.connect(host, port, connectionTimeout)) {
            constexpr int SERVER_BUFFER_SIZE = 1024;
            char serverBuffer[SERVER_BUFFER_SIZE];
            snprintf(serverBuffer, SERVER_BUFFER_SIZE,
                "SOURCE %s /%s\r\nSource-Agent: NTRIP %s/App Version %s\r\n\r\n",
                pw, mnt, settings["ntrip_sName"].as<const char*>(), FIRMWARE_VERSION);

            client.write(serverBuffer, strlen(serverBuffer));

            // Wait for and verify response
            if (verifyServerResponse(client)) {
                status.connected = true;
                status.reconnectAttempts = 0;
                status.lastError = "";
                status.connectionOpenedAt = millis();
                infof("NTRIP %s - Connected to %s", isPrimary ? "Primary" : "Secondary", host);
                return true;
            }
        }
    }
    return false;
}

void SFE_UBLOX_GNSS::processRTCM(uint8_t incoming)
{
    lastRtcmData_ms = millis();
    if (NtripPrimaryStatus.connected) {
        client.write(incoming); // Send data to NTRIP client
        NtripPrimaryStatus.bytesSent++;
    }
    if (NtripSecondaryStatus.connected) {
        client2.write(incoming); // Send data to NTRIP client
        NtripSecondaryStatus.bytesSent++;
    }
}

void ntrip_handle_init() {
    infof("NTRIP: Primary %s, Secondary %s",
          settings["enableCaster1"].as<bool>() ? "Enabled" : "Disabled",
          settings["enableCaster2"].as<bool>() ? "Enabled" : "Disabled");

    xTaskCreate(NTRIPTask, "NTRIPTask", 8192, // Stack size
                nullptr, // Task parameters
                1, // Task priority
                nullptr // Task handle
    );
}

bool verifyServerResponse(WiFiClient& client) {
    // Wait for response with timeout
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > connectionTimeout) {
            error("Client Timeout");
            return false;
        }
        delay(10);
        yield();
    }

    // Check reply
    char response[512];
    int responseSpot = 0;
    while (client.available()) {
        response[responseSpot++] = client.read();
        if (strstr(response, "200")) { //Look for 'ICY 200 OK'
            debug("Caster response OK");
            return true;
        }
        if (responseSpot == 512 - 1)
            break;
        yield();
    }
    response[responseSpot] = '\0';

    errorf("Failed to verify server response: %s", response);
    return false;
}

[[noreturn]] void NTRIPTask(void *pvParameter) {
    for (;;) {
        // Handle NTRIP communications
        handleNTRIP();
        delay(1);
    }
}
