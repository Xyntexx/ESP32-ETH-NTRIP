#include <core/defines.h>
#include <Arduino.h>
#include "ntrip.h"
#include "hardware/gps.h"
#include <WebServer_ESP32_SC_W6100.hpp>
#include "utils/log.h"
#include "rtcmbuffer.h"

// Base64 encoding table
const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Simple base64 encoding function
String base64_encode(const String& input) {
    String result;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    int in_len = input.length();
    const char* bytes_to_encode = input.c_str();

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4; i++) {
                result += base64_table[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++) {
            result += base64_table[char_array_4[j]];
        }

        while((i++ < 3)) {
            result += '=';
        }
    }

    return result;
}

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
NTRIPStatus NtripPrimaryStatus   = {false, 0,  "", 0, 0, 1};  // Default to NTRIP 1.0
NTRIPStatus NtripSecondaryStatus = {false, 0,  "", 0, 0, 1};  // Default to NTRIP 1.0
unsigned long lastReport_ms      = 0;
unsigned long lastRtcmData_ms    = 0;  // Track when we last received RTCM data

// Mutex for thread-safe status access
SemaphoreHandle_t statusMutex = NULL;

bool ntrip_inited = false;

[[noreturn]] void NTRIPTask(void *pvParameter);

bool checkAndConnect(WiFiClient& client, NTRIPStatus& status, const bool isPrimary);

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

NTRIPError verifyServerResponse(WiFiClient& client) {
    // Wait for response with timeout
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if ((unsigned long)(millis() - timeout) > connectionTimeout) {
            error("Client Timeout");
            client.stop(); // Ensure client is properly stopped
            return NTRIPError::TIMEOUT;
        }
        delay(10);
        yield();
    }

    // Check reply - safer buffer handling
    constexpr int MAX_RESPONSE_SIZE = 512;
    char response[MAX_RESPONSE_SIZE];
    int responseSpot = 0;
    
    // Read response with buffer protection
    while (client.available() && responseSpot < MAX_RESPONSE_SIZE - 1) {
        response[responseSpot++] = client.read();
        if (responseSpot >= 3) { // At least 3 characters to check for "200"
            // Null-terminate the partial buffer for string operations
            response[responseSpot] = '\0';
            
            // Accept either "ICY 200 OK" or any response with "200" status code
            if (strstr(response, "ICY 200") || // Standard NTRIP v1 response
                strstr(response, "HTTP/1.1 200") || // HTTP style response
                strstr(response, "HTTP/1.0 200") || // HTTP style response
                strstr(response, "200 OK")) { // Generic 200 OK
                
                // Found successful response, but keep reading to clear buffer
                while (client.available()) {
                    client.read(); // Clear remaining bytes
                    yield();
                }
                debug("Caster response OK");
                return NTRIPError::NONE;
            }
            
            // Check for authentication errors
            if (strstr(response, "401 Unauthorized") || 
                strstr(response, "403 Forbidden")) {
                errorf("Authentication failed: %s", response);
                client.stop();
                return NTRIPError::AUTH_FAILED;
            }
        }
        yield();
    }
    
    // Ensure null termination
    response[responseSpot < MAX_RESPONSE_SIZE ? responseSpot : MAX_RESPONSE_SIZE - 1] = '\0';

    errorf("Failed to verify server response: %s", response);
    client.stop(); // Ensure client is properly stopped
    return NTRIPError::INVALID_RESPONSE;
}

void handleError(bool isPrimary, NTRIPError error) {
    String prefix = isPrimary ? "Primary" : "Secondary";
    String errorMsg = getErrorMessage(error);

    errorf("NTRIP %s - %s", prefix.c_str(), errorMsg.c_str());

    if (isPrimary) {
        NtripPrimaryStatus.lastError = errorMsg;
        // Only increment reconnect attempts for connection related errors
        if (error == NTRIPError::CONNECTION_FAILED || 
            error == NTRIPError::TIMEOUT || 
            error == NTRIPError::AUTH_FAILED) {
            NtripPrimaryStatus.reconnectAttempts++;
        }
    } else {
        NtripSecondaryStatus.lastError = errorMsg;
        // Only increment reconnect attempts for connection related errors
        if (error == NTRIPError::CONNECTION_FAILED || 
            error == NTRIPError::TIMEOUT || 
            error == NTRIPError::AUTH_FAILED) {
            NtripSecondaryStatus.reconnectAttempts++;
        }
    }
}

bool stopNTRIP(WiFiClient& client, bool isPrimary)
{
    debug(isPrimary ? "Disconnecting primary NTRIP..." : "Disconnecting secondary NTRIP...");

    client.stop();
    infof("NTRIP %s - Disconnected", isPrimary ? "Primary" : "Secondary");
    
    if (isPrimary) {
        // Preserve reconnection attempts and last error when disconnecting
        NTRIPStatus newStatus = NtripPrimaryStatus;
        newStatus.connected = false;
        newStatus.bytesSent = 0;
        newStatus.connectionOpenedAt = 0;
        NtripPrimaryStatus = newStatus;
    } else {
        // Preserve reconnection attempts and last error when disconnecting
        NTRIPStatus newStatus = NtripSecondaryStatus;
        newStatus.connected = false;
        newStatus.bytesSent = 0;
        newStatus.connectionOpenedAt = 0;
        NtripSecondaryStatus = newStatus;
    }
    return true;
}


// Replace the checkConnectionHealth function with this improved version
NTRIPError checkConnectionHealth(WiFiClient& client, NTRIPStatus& status, bool isPrimary) {
    const unsigned long currentMillis = millis();
    
    // Check if the client is connected at all - always check this regardless of stability period
    if (!client.connected()) {
        handleError(isPrimary, NTRIPError::CONNECTION_FAILED);
        return NTRIPError::CONNECTION_FAILED;
    }
    
    // If we're in the stability period, only perform basic checks
    bool inStabilityPeriod = (unsigned long)(currentMillis - status.connectionOpenedAt) < connectionStabilityTimeout_ms;
    
    // During stability period, we're more lenient with connection checks
    if (inStabilityPeriod) {
        return NTRIPError::NONE;
    }
    
    return NTRIPError::NONE;
}

// Handle all RTCM Checking related things. If return is true, the NTRIP connection should be opened or continued. if false, the NTRIP connection should be closed immediately.
NTRIPError RTCMCheck() {
    // If RTCM checks are disabled, always allow connection
    if (!settings["rtcmChk"].as<bool>()) {
        return NTRIPError::NONE;
    }

    // Don't allow connection during survey mode
    if (currentGPSStatus.surveyInActive) {
        debugf("NTRIP - Survey in active, not connecting");
        return NTRIPError::SURVEY_IN_ACTIVE;
    }

    auto last_rtcm_data_ms = lastRtcmData_ms;
    auto time_now_ms = millis();

    // Fixed: Handle millis() overflow properly with unsigned arithmetic
    // If we haven't received RTCM data in the timeout period, don't allow connection
    if ((unsigned long)(time_now_ms - last_rtcm_data_ms) > maxTimeBeforeHangup_ms) {
        debugf("NTRIP - RTCM timeout: last data %lu ms ago. Timestamp:%lu Prev RTCM:%lu",
               (unsigned long)(time_now_ms - last_rtcm_data_ms), time_now_ms, last_rtcm_data_ms);
        return NTRIPError::RTCM_TIMEOUT;
    }

    // If we have recent RTCM data, allow connection
    return NTRIPError::NONE;
}

void handleNTRIP()
{
    static unsigned long previousConnectAttempt = 0;
    // Remove duplicate counter as we already have reconnectAttempts in the status structs
    const unsigned long currentMillis = millis();

    // Calculate the appropriate delay based on max of both connection attempts
    int maxAttempts = max(NtripPrimaryStatus.reconnectAttempts, NtripSecondaryStatus.reconnectAttempts);
    unsigned long connectInterval = (maxAttempts >= maxReconnectAttempts) ? slowReconnectDelay : reconnectDelay;

    // Check if we should be connected based on RTCM data
    NTRIPError rtcmError = RTCMCheck();
    
    // If RTCM check fails, disconnect any existing connections
    if (rtcmError != NTRIPError::NONE) {
        if (NtripPrimaryStatus.connected) {
            handleError(true, rtcmError);
            stopNTRIP(client, true);
        }
        
        if (NtripSecondaryStatus.connected) {
            handleError(false, rtcmError);
            stopNTRIP(client2, false);
        }
        return; // Don't proceed with connection attempts
    }

    // Monitor existing connections only if RTCM check passed
    NTRIPError primaryError = NtripPrimaryStatus.connected ? 
        checkConnectionHealth(client, NtripPrimaryStatus, true) : NTRIPError::NONE;
    
    if (primaryError != NTRIPError::NONE && NtripPrimaryStatus.connected) {
        handleError(true, primaryError);
        stopNTRIP(client, true);
    }
    
    NTRIPError secondaryError = NtripSecondaryStatus.connected ? 
        checkConnectionHealth(client2, NtripSecondaryStatus, false) : NTRIPError::NONE;
    
    if (secondaryError != NTRIPError::NONE && NtripSecondaryStatus.connected) {
        handleError(false, secondaryError);
        stopNTRIP(client2, false);
    }

    // Try to connect if not already connected and enough time has passed
    // Handle millis() overflow by using subtraction (works correctly even with overflow)
    bool timeToReconnect = (previousConnectAttempt == 0) || 
                          ((unsigned long)(currentMillis - previousConnectAttempt) >= connectInterval);
                          
    if (timeToReconnect) {
        previousConnectAttempt = currentMillis;
        
        // Only try to connect primary if not connected
        if (!NtripPrimaryStatus.connected) {
            if (!checkAndConnect(client, NtripPrimaryStatus, true)) {
                // Connection attempt failed, increment is handled in handleError
            }
        }
        
        // Only try to connect secondary if not connected
        if (!NtripSecondaryStatus.connected) {
            if (!checkAndConnect(client2, NtripSecondaryStatus, false)) {
                // Connection attempt failed, increment is handled in handleError
            }
        }
    }

    // Report statistics every 10 seconds
    // Handle millis() overflow safely
    if ((unsigned long)(currentMillis - lastReport_ms) > 10000) {
        lastReport_ms = currentMillis;
        debugf("NTRIP Status - Primary: %s (%d bytes, %d attempts), Secondary: %s (%d bytes, %d attempts)",
            NtripPrimaryStatus.connected ? "Connected" : "Disconnected",
            NtripPrimaryStatus.bytesSent,
            NtripPrimaryStatus.reconnectAttempts,
            NtripSecondaryStatus.connected ? "Connected" : "Disconnected",
            NtripSecondaryStatus.bytesSent,
            NtripSecondaryStatus.reconnectAttempts);
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

    // Validate connection state consistency
    bool clientConnected = client.connected();
    bool statusConnected = status.connected;

    // If there's a mismatch, trust the actual client state
    if (clientConnected != statusConnected) {
        warning(isPrimary ? "Primary connection state mismatch - synchronizing" :
                           "Secondary connection state mismatch - synchronizing");
        if (!clientConnected && statusConnected) {
            // Client disconnected but status thinks it's connected - update status
            status.connected = false;
            status.connectionOpenedAt = 0;
        } else if (clientConnected && !statusConnected) {
            // Client connected but status thinks it's not - shouldn't happen, but sync
            status.connected = true;
            status.connectionOpenedAt = millis();
        }
    }

    // If already connected, nothing to do
    if (clientConnected) {
        return true;
    }

    // Check if RTCM data is available
    NTRIPError rtcmError = RTCMCheck();
    if (rtcmError != NTRIPError::NONE) {
        handleError(isPrimary, rtcmError);
        return false; // Don't attempt to connect if RTCM check fails
    }

    // If we get here, we should attempt to connect
    const char* host = isPrimary ? settings["casterHost1"] : settings["casterHost2"];
    uint16_t port = isPrimary ? settings["casterPort1"].as<uint16_t>() : settings["casterPort2"].as<uint16_t>();
    const char* pw = isPrimary ? settings["rtk_mntpnt_pw1"] : settings["rtk_mntpnt_pw2"];
    const char* user = isPrimary ? settings["rtk_mntpnt_user1"] : settings["rtk_mntpnt_user2"];
    const char* mnt = isPrimary ? settings["rtk_mntpnt1"] : settings["rtk_mntpnt2"];
    
    // Get the appropriate version setting for this mount point
    int ntripVersion;
    
    if (isPrimary) {
        if (settings.containsKey("ntripVersion1")) {
            ntripVersion = settings["ntripVersion1"].as<int>();
        } else {
            ntripVersion = 1; // Default to 1.0
        }
    } else {
        if (settings.containsKey("ntripVersion2")) {
            ntripVersion = settings["ntripVersion2"].as<int>();
        } else {
            ntripVersion = 1; // Default to 1.0
        }
    }

    debugf("NTRIP %s - Attempting connection to %s:%d (Version %d)", isPrimary ? "Primary" : "Secondary", host, port, ntripVersion);

    if (client.connect(host, port, connectionTimeout)) {
        constexpr int SERVER_BUFFER_SIZE = 1024;
        char serverBuffer[SERVER_BUFFER_SIZE];
        
        if (ntripVersion == 2) {
            // NTRIP Rev 2.0: Use HTTP/1.1 POST with Basic Auth
            String auth = String(user) + ":" + String(pw);
            String base64Auth = base64_encode(auth);

            snprintf(serverBuffer, SERVER_BUFFER_SIZE,
                "POST /%s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: NTRIP %s/App Version %s\r\n"
                "Authorization: Basic %s\r\n"
                "Ntrip-Version: Ntrip/2.0\r\n"
                "Connection: close\r\n\r\n",
                mnt, host,
                settings["ntrip_sName"].as<const char*>(),
                FIRMWARE_VERSION,
                base64Auth.c_str()
            );
        } else {
            // NTRIP Rev 1.0: Custom 'SOURCE' method
            snprintf(serverBuffer, SERVER_BUFFER_SIZE,
                "SOURCE %s /%s\r\n"
                "Source-Agent: NTRIP %s/App Version %s\r\n\r\n",
                pw, mnt,
                settings["ntrip_sName"].as<const char*>(),
                FIRMWARE_VERSION
            );
        }

        client.write(serverBuffer, strlen(serverBuffer));

        // Wait for and verify response
        NTRIPError responseError = verifyServerResponse(client);
        
        if (responseError == NTRIPError::NONE) {
            status.connected = true;
            status.reconnectAttempts = 0; // Reset attempts on successful connection
            status.lastError = "";
            status.connectionOpenedAt = millis();
            status.protocolVersion = ntripVersion;  // Store the protocol version
            infof("NTRIP %s - Connected to %s", isPrimary ? "Primary" : "Secondary", host);
            return true;
        } else {
            // Connection failed during verification
            handleError(isPrimary, responseError);
            return false;
        }
    } else {
        // Connection failed to establish
        handleError(isPrimary, NTRIPError::CONNECTION_FAILED);
        return false;
    }
}

void send_rtcm(const uint8_t *data, const int len) {
    // Reduced verbosity - only log once per message, not every byte
    if (len > 0) {
        debugf("RTCM data received: %d bytes", len);
    }

    // Thread-safe access to status
    if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool primaryConnected = NtripPrimaryStatus.connected;
        int primaryVersion = NtripPrimaryStatus.protocolVersion;
        bool secondaryConnected = NtripSecondaryStatus.connected;
        int secondaryVersion = NtripSecondaryStatus.protocolVersion;

        xSemaphoreGive(statusMutex);

        if (primaryConnected) {
            if (primaryVersion == 2) {
                // Format as HTTP chunked transfer
                char chunkHeader[10];
                snprintf(chunkHeader, sizeof(chunkHeader), "%X\r\n", len);
                client.write((const uint8_t *)chunkHeader, strlen(chunkHeader));
                client.write(data, len);
                client.write("\r\n", 2);
            } else {
                // NTRIP 1.0: send raw RTCM data
                client.write(data, len);
            }

            // Update bytes sent with mutex protection
            if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                NtripPrimaryStatus.bytesSent += len;
                xSemaphoreGive(statusMutex);
            }
        }

        if (secondaryConnected) {
            if (secondaryVersion == 2) {
                // Format as HTTP chunked transfer
                char chunkHeader[10];
                snprintf(chunkHeader, sizeof(chunkHeader), "%X\r\n", len);
                client2.write((const uint8_t *)chunkHeader, strlen(chunkHeader));
                client2.write(data, len);
                client2.write("\r\n", 2);
            } else {
                // NTRIP 1.0: send raw RTCM data
                client2.write(data, len);
            }

            // Update bytes sent with mutex protection
            if (xSemaphoreTake(statusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                NtripSecondaryStatus.bytesSent += len;
                xSemaphoreGive(statusMutex);
            }
        }
    }
}

void SFE_UBLOX_GNSS::processRTCM(uint8_t incoming) {
    if (!ntrip_inited) {
        return;
    }
    // Removed per-byte debug logging to reduce verbosity
    rtcmbuffer::process_byte(incoming,&send_rtcm);
    lastRtcmData_ms = millis();
}

void ntrip_handle_init() {
    // Create mutex for thread-safe status access
    statusMutex = xSemaphoreCreateMutex();
    if (statusMutex == NULL) {
        error("Failed to create NTRIP status mutex");
        return;
    }

    rtcmbuffer::init();
    xTaskCreate(NTRIPTask, "NTRIPTask", 8192, // Stack size
                nullptr, // Task parameters
                NTRIP_TASK_PRIORITY, // Task priority
                nullptr // Task handle
    );
    ntrip_inited = true;
}

[[noreturn]] void NTRIPTask(void *pvParameter) {
    for (;;) {
        // Handle NTRIP communications
        handleNTRIP();
        delay(1);
    }
}
