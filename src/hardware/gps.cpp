#include <Arduino.h>
#include "gps.h"
#include "utils/log.h"
#include "utils/settings.h"
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>

SFE_UBLOX_GNSS myGNSS;

constexpr int GPS_RX_PIN = 0;
constexpr int GPS_TX_PIN = 1;
constexpr size_t test_bauds_len = 3;
constexpr int test_bauds[test_bauds_len] = {38400, 115200, 230400};
constexpr int selected_baud = 115200;

bool gpsConnected = false;
unsigned long gpsInitTime;
String gpsStatusSting;

GPSStatusStruct currentGPSStatus;

[[noreturn]] void gpsStatusTask(void *pvParameters);
[[noreturn]] void gps_uart_check_task(void *pvParameters);

bool prev_survey_in_active = false;

bool fast_uart_handle = false;

void disable_fast_uart();
void enable_fast_uart();
bool configureGPS();
bool updateGPSStatus();

void initializeGPS() {
    disable_fast_uart();
    bool resp = false;
    debug("Initializing GPS...");
    for (const int test_baud : test_bauds) {
        debugf("Testing baud rate: %d", test_baud);
        Serial1.end();
        Serial1.setRxBufferSize(1024 * 5);
        Serial1.begin(test_baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
        delay(1000);
        if ((resp = myGNSS.begin(Serial1, defaultMaxWait, false))) {
            break;
        }
    }

    if (!resp) {
        error("GPS - Not detected");
        gpsConnected = false;
        return;
    }
    // Configure the GPS module
    gpsConnected = configureGPS();

    if (gpsConnected) {
        gpsInitTime = millis();
    }
    // Start task
    xTaskCreate(gps_uart_check_task, "gps_uart_check_task", 10000, nullptr, configMAX_PRIORITIES - 2, nullptr);
    delay(1000);
    xTaskCreate(gpsStatusTask, "gpsStatusTask", 4096, nullptr, configMAX_PRIORITIES - 3, nullptr);
}

bool configureGPS() {
    //myGNSS.enableDebugging(USBSerial);
    bool result = false;
    // UBX+RTCM3 is not a valid option so we enable all three.
    // myGNSS.setPortOutput(COM_PORT_UART1, COM_TYPE_UBX | COM_TYPE_NMEA | COM_TYPE_RTCM3); //Set the UART port to
    // output UBX only (turn off NMEA and RTCM)

    myGNSS.setNavigationFrequency(1);  // Set output in Hz. RTCM rarely benefits from >1Hz.

    bool response = myGNSS.setUART1Output(COM_TYPE_UBX | COM_TYPE_NMEA |
                                          COM_TYPE_RTCM3);  // Set the UART port to output RTCM3 and NMEA
    if (response == false) {
        error("GPS - Failed to set UART1 output.");
    }
    response = myGNSS.setUSBOutput(COM_TYPE_UBX | COM_TYPE_NMEA |
                                   COM_TYPE_RTCM3);  // Set the UART port to output UBX only (turn off NMEA and RTCM)
    if (response == false) {
        error("GPS - Failed to set USB output.");
    }

    // update uart1 baud rate
    debugf("Setting UART1 baud rate to %d", selected_baud);
    myGNSS.setSerialRate(selected_baud, COM_PORT_UART1);  // Set the UART port to fast baud rate
    Serial1.end();
    Serial1.setRxBufferSize(1024 * 5);
    Serial1.begin(selected_baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    // Disable all NMEA sentences
    response &= myGNSS.disableNMEAMessage(UBX_NMEA_GGA, COM_PORT_UART1);
    response &= myGNSS.disableNMEAMessage(UBX_NMEA_GSA, COM_PORT_UART1);
    response &= myGNSS.disableNMEAMessage(UBX_NMEA_GSV, COM_PORT_UART1);
    response &= myGNSS.disableNMEAMessage(UBX_NMEA_RMC, COM_PORT_UART1);
    response &= myGNSS.disableNMEAMessage(UBX_NMEA_GST, COM_PORT_UART1);
    response &= myGNSS.disableNMEAMessage(UBX_NMEA_GLL, COM_PORT_UART1);
    response &= myGNSS.disableNMEAMessage(UBX_NMEA_VTG, COM_PORT_UART1);
    if (response == false) {
        error("GPS - Failed to disable NMEA.");
    }

    // Enable necessary RTCM sentences
    response &= myGNSS.enableRTCMmessage(UBX_RTCM_1005, COM_PORT_UART1,
                                         1);  // Enable message 1005 to output through UART2, message every second
    response &= myGNSS.enableRTCMmessage(UBX_RTCM_1074, COM_PORT_UART1, 1);
    response &= myGNSS.enableRTCMmessage(UBX_RTCM_1084, COM_PORT_UART1, 1);
    response &= myGNSS.enableRTCMmessage(UBX_RTCM_1094, COM_PORT_UART1, 1);
    response &= myGNSS.enableRTCMmessage(UBX_RTCM_1124, COM_PORT_UART1, 1);
    response &= myGNSS.enableRTCMmessage(UBX_RTCM_1230, COM_PORT_UART1, 10);  // Enable message every 10 seconds

    // Enable RTCM messages on USB
    response &= myGNSS.enableRTCMmessage(UBX_RTCM_1005, COM_PORT_USB,
                                         1);  // Enable message 1005 to output through UART2, message every second
    response &= myGNSS.enableRTCMmessage(UBX_RTCM_1074, COM_PORT_USB, 1);
    response &= myGNSS.enableRTCMmessage(UBX_RTCM_1084, COM_PORT_USB, 1);
    response &= myGNSS.enableRTCMmessage(UBX_RTCM_1094, COM_PORT_USB, 1);
    response &= myGNSS.enableRTCMmessage(UBX_RTCM_1124, COM_PORT_USB, 1);
    response &= myGNSS.enableRTCMmessage(UBX_RTCM_1230, COM_PORT_USB, 10);  // Enable message every 10 seconds

    if (response == false) {
        error("GPS - Failed to enable RTCM.");
    }

    // automatic message reporting for gps status:
    response = true;

    response &= myGNSS.setAutoHPPOSLLH(true);
    response &= myGNSS.setAutoPVT(true);
    response &= myGNSS.setAutoNAVHPPOSECEF(true);
    response &= myGNSS.setAutoNAVSVIN(true);

    if (response == false) {
        error("GPS - Failed to set automatic messages.");
    }

    int64_t ecefX = settings["ecefX"].as<int64_t>();
    int64_t ecefY = settings["ecefY"].as<int64_t>();
    int64_t ecefZ = settings["ecefZ"].as<int64_t>();

    int32_t ecefX_cm = (ecefX / 100);
    int32_t ecefY_cm = (ecefY / 100);
    int32_t ecefZ_cm = (ecefZ / 100);
    int8_t ecefX_0_1mm = (ecefX % 100);
    int8_t ecefY_0_1mm = (ecefY % 100);
    int8_t ecefZ_0_1mm = (ecefZ % 100);

    debugf("ecefX: %d.%02dcm", ecefX_cm, ecefX_0_1mm);
    debugf("ecefY: %d.%02dcm", ecefY_cm, ecefY_0_1mm);
    debugf("ecefZ: %d.%02dcm", ecefZ_cm, ecefZ_0_1mm);

    if (ecefX == 0 && ecefY == 0 && ecefZ == 0) {
        info("GPS - Static position not set. Using Rover mode.");
        response = myGNSS.setSurveyMode(0, 0, 0);  // Disable survey mode
    } else {
        debugf("Setting static position to %d.%02d, %d.%02d, %d.%02d", 
               ecefX_cm, ecefX_0_1mm, ecefY_cm, ecefY_0_1mm, ecefZ_cm, ecefZ_0_1mm);
        response = myGNSS.setStaticPosition(ecefX_cm, ecefX_0_1mm, ecefY_cm, ecefY_0_1mm, ecefZ_cm, ecefZ_0_1mm, false);
    }

    if (response == false) {
        error("GPS - Failed to set GPS mode.");
    } else {
        debug("GPS - Module configuration complete");
        result = true;
    }
    fast_uart_handle = true;
    return result;
}

String appendLeadingZero(int input) {
    if (input < 10) {
        return "0" + String(input);
    }
    return String(input);
}

// Function to start Survey-in mode
bool startSurveyMode(uint16_t observationTime, float requiredAccuracy) {
    disable_fast_uart();
    if (!gpsConnected) {
        error("GPS - Not connected.");
        enable_fast_uart();
        return false;
    }
    bool response = true;
    // response = myGNSS.setSurveyMode(0, 0, 0);  // Disable survey mode
    if (response == false) {
        error("GPS - Failed to stop Survey-in mode.");
        enable_fast_uart();
        return false;
    }

    infof("GPS - Starting Survey-in mode for %d seconds with accuracy %.2f meters...",
          observationTime, requiredAccuracy);

    // Set ZED-F9P to Survey-in mode
    response = myGNSS.setSurveyMode(1, observationTime, requiredAccuracy);
    // accuracy
    if (response == false) {
        error("GPS - Failed to set Survey-in mode.");
        enable_fast_uart();
        return false;
    }
    info("GPS - Survey-in mode started.");
    return true;
}

void stopSurveyMode() {
    info("GPS - Stopping Survey-in mode...");

    // Set ZED-F9P to Survey-in mode
    bool resp = myGNSS.setSurveyMode(0, 0, 0);  // Minimum 600s (10 min) and 2.0m
    // accuracy

    if (resp == false) {
        error("GPS - Failed to stop Survey-in mode.");
    } else {
        info("GPS - Survey-in mode stopped.");
    }
    enable_fast_uart();
}

bool saveSurveyPosition() {
    disable_fast_uart();
    if (myGNSS.getSurveyInValid()) {
        const int64_t x = static_cast<int64_t>(myGNSS.getHighResECEFX()) * 100 + myGNSS.getHighResECEFXHp();
        const int64_t y = static_cast<int64_t>(myGNSS.getHighResECEFY()) * 100 + myGNSS.getHighResECEFYHp();
        const int64_t z = static_cast<int64_t>(myGNSS.getHighResECEFZ()) * 100 + myGNSS.getHighResECEFZHp();
        writeSettings("ecefX", x);
        writeSettings("ecefY", y);
        writeSettings("ecefZ", z);
        settings["ecefX"] = x;
        settings["ecefY"] = y;
        settings["ecefZ"] = z;

        const int32_t x_cm = (x / 100);
        const int32_t y_cm = (y / 100);
        const int32_t z_cm = (z / 100);
        const int8_t x_0_1mm = (x % 100);
        const int8_t y_0_1mm = (y % 100);
        const int8_t z_0_1mm = (z % 100);

        bool response = myGNSS.setStaticPosition(x_cm, x_0_1mm, y_cm, y_0_1mm, z_cm, z_0_1mm, false);
        if (response == false) {
            error("GPS - Failed to set static position.");
        } else {
            info("GPS - Static position set.");
        }
        enable_fast_uart();
        return true;
    }
    enable_fast_uart();
    return false;
}

String gpsStatusString(const GPSStatusStruct &currentGPSStatus_) {
    switch (currentGPSStatus_.gpsMode) {
        case GPSMode::ROVER:
            return "Rover mode";
        case GPSMode::SURVEY_IN:
            return "Survey-in mode";
        case GPSMode::FIXED:
            return "Fixed mode";
        case GPSMode::UNKNOWN:
        default:
            return "Unknown mode";
    }
}

// add a lock to prevent multiple tasks from accessing the GPS status at the same time
long last_time = 0;
bool updateGPSStatus() {
    // Check if survey parameters are set and start survey mode if needed
    if (currentGPSStatus.requestedSurveyTime > 0 && currentGPSStatus.requestedSurveyAccuracy > 0) {
        uint16_t observationTime = currentGPSStatus.requestedSurveyTime;
        float requiredAccuracy = currentGPSStatus.requestedSurveyAccuracy;

        // Clear the requested parameters after reading them
        currentGPSStatus.requestedSurveyTime = 0;
        currentGPSStatus.requestedSurveyAccuracy = 0.0f;

        // Start survey mode
        if (!startSurveyMode(observationTime, requiredAccuracy)) {
            error("GPS - Failed to start survey mode");
        }
    }

    // Update the global GPS status
    currentGPSStatus.gpsConnected = gpsConnected;
    currentGPSStatus.status_message = currentGPSStatus.gpsConnected ? "Connected" : "Disconnected";
    currentGPSStatus.latitude = myGNSS.getHighResLatitude() / 10000000.0 + myGNSS.getHighResLatitudeHp() / 1000000000.0;
    currentGPSStatus.longitude =
        myGNSS.getHighResLongitude() / 10000000.0 + myGNSS.getHighResLongitudeHp() / 1000000000.0;
    currentGPSStatus.altitude = myGNSS.getAltitude() / 1000.0;
    currentGPSStatus.x = myGNSS.getHighResECEFX() / 10.0 + myGNSS.getHighResECEFXHp() / 100.0;
    currentGPSStatus.y = myGNSS.getHighResECEFY() / 10.0 + myGNSS.getHighResECEFYHp() / 100.0;
    currentGPSStatus.z = myGNSS.getHighResECEFZ() / 10.0 + myGNSS.getHighResECEFZHp() / 100.0;

    currentGPSStatus.surveyInActive = myGNSS.getSurveyInActive();
    currentGPSStatus.surveyInValid = myGNSS.getSurveyInValid();
    currentGPSStatus.surveyInObservationTime = myGNSS.getSurveyInObservationTime();
    currentGPSStatus.surveyInMeanAccuracy = myGNSS.getSurveyInMeanAccuracy();
    currentGPSStatus.satellites = myGNSS.getSIV();

    bool response = false;
    disable_fast_uart();
    UBX_CFG_TMODE3_data_t tmode3_data;
    //auto start_time = millis();
    const int maxRetries = 5;
    const int retryTimeout = 1000; // 1 second timeout
    
    for (int retry = 0; retry < maxRetries; retry++) {
        response = myGNSS.getSurveyMode(&tmode3_data, retryTimeout); 
        if (response) {
            break; // Success, exit the retry loop
        }
    }
    
    if (!response) {
        error("GPS - Failed to get Survey-in mode.");
        enable_fast_uart();
        return false;
    }
    //debugf("Getting Survey-in mode took %d ms", millis() - start_time);
    enable_fast_uart();
    currentGPSStatus.gpsMode = static_cast<GPSMode>(tmode3_data.flags.bits.mode);

    gpsStatusSting = gpsStatusString(currentGPSStatus);

    currentGPSStatus.gpsModeString = gpsStatusSting.c_str();

    // If survey-in mode has just completed, save the position
    if (prev_survey_in_active && !currentGPSStatus.surveyInActive) {
        info("GPS - Survey-in completed. Saving position...");
        saveSurveyPosition();
    }
    prev_survey_in_active = currentGPSStatus.surveyInActive;
    return true;
}

void enable_fast_uart() {
    fast_uart_handle = true;
}

void disable_fast_uart() {
    fast_uart_handle = false;
}

// Update the gpsStatusTask to populate the currentGPSStatus
[[noreturn]] void gpsStatusTask(void *pvParameters) {
    for (;;) {
        updateGPSStatus();
        delay(1000);  // Wait for 1 second
    }
}

[[noreturn]] void gps_uart_check_task(void *pvParameters){
    for (;;) {
        // Process GNSS data if any connection is active
        if (fast_uart_handle) {
            myGNSS.checkUblox();
            if (!Serial1.available()) {
                vTaskDelay(1/portTICK_PERIOD_MS);
            }
        } else {
            vTaskDelay(1);
        }
    }
}