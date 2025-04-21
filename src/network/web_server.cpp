#include <Arduino.h>
#include "core/defines.h"

#include "AsyncWebServer_ESP32_SC_W6100.h"

#include "hardware/gps.h"
#include "utils/settings.h"
#include <http_parser.h>
#include "ntrip.h"
#include "web_server.h"
#include <Update.h>
#include "utils/system_status.h"
#include "utils/log.h"

// HTTP Related
AsyncWebServer server(80);


static void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

// Add these handler functions before initializeWebServer()
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        // Start update
        debugf("Update Start: %s", filename.c_str());
        info("OTA: Starting update");
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(USBSerial);
            request->send(400, "text/plain", "OTA start failed");
            return;
        }
    }

    // Write update chunk
    if (Update.write(data, len) != len) {
        Update.printError(USBSerial);
        request->send(400, "text/plain", "OTA write failed");
        return;
    }

    if (final) {
        if (!Update.end(true)) {
            Update.printError(USBSerial);
            request->send(400, "text/plain", "OTA end failed");
            return;
        }
        info("OTA: Update successful");
        request->send(200, "text/plain", "Update successful. Rebooting...");
        delay(500);
        ESP.restart();
    }
}

// Add this helper function to calculate uptime string from milliseconds
String calculateUptime(unsigned long milliseconds) {
    unsigned long seconds = milliseconds / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;

    hours %= 24;
    minutes %= 60;
    seconds %= 60;

    char uptimeStr[50];
    if (days > 0) {
        snprintf(uptimeStr, sizeof(uptimeStr), "%lud %02luh %02lum", days, hours, minutes);
    } else if (hours > 0) {
        snprintf(uptimeStr, sizeof(uptimeStr), "%luh %02lum", hours, minutes);
    } else {
        snprintf(uptimeStr, sizeof(uptimeStr), "%lum %02lus", minutes, seconds);
    }
    return String(uptimeStr);
}

// Start web server
void initializeWebServer()
{
    info("Starting webserver");
    server.begin();

    for (const auto & webpage : webpages)
    {
        server.on(webpage.filename, HTTP_GET, [&webpage](AsyncWebServerRequest *request)
                  { request->send(200, webpage.content_type, (const char *)webpage.data); });
    }

    server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                request->send(200, "text/html", "message");
                info("Rebooting...");
                ESP.restart(); });

    server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                  String message;
                  serializeJson(settings, message);
                  request->send(200, "text/plain", message); });
    server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", getLog()); });
    // Start Survey-in mode
    server.on("/startSurvey", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Check if time parameter is provided
        if (request->hasParam("time")) {
            currentGPSStatus.requestedSurveyTime = request->getParam("time")->value().toInt();
        }

        // Check if accuracy parameter is provided
        if (request->hasParam("accuracy")) {
            currentGPSStatus.requestedSurveyAccuracy = request->getParam("accuracy")->value().toFloat();
        }

        infof("Survey-in parameters set: %d seconds, %.2f meters",
              currentGPSStatus.requestedSurveyTime,
              currentGPSStatus.requestedSurveyAccuracy);
        request->send(200, "text/plain", "Survey parameters saved");
    });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                  String message;
                  StaticJsonDocument<512> status;

                  // Add version information
                  status["firmwareVersion"] = FIRMWARE_VERSION;
                  status["buildDate"]       = BUILD_DATE;
                  status["uptime"]          = getUptimeString();
                  status["timestamp"]       = millis();

                  // Add NTRIP connection status
                  status["enableCaster1"] = settings["enableCaster1"].as<bool>();
                  status["enableCaster2"] = settings["enableCaster2"].as<bool>();

                  // Use connection opened times for status
                  status["ntripConnected1"] = NtripPrimaryStatus.connected;
                  status["ntripConnected2"] = NtripSecondaryStatus.connected;

                  // Calculate uptimes if connected
                  unsigned long currentMillis = millis();

                  if (NtripPrimaryStatus.connectionOpenedAt > 0) {
                      status["ntripUptime1"] = calculateUptime(currentMillis - NtripPrimaryStatus.connectionOpenedAt);
                  }

                  if (NtripSecondaryStatus.connectionOpenedAt > 0) {
                      status["ntripUptime2"] = calculateUptime(currentMillis - NtripSecondaryStatus.connectionOpenedAt);
                  }

                  // Rest of the status fields...
                  status["gpsStatusString"] = currentGPSStatus.status_message;
                  status["gpsLatitude"] = serialized(String(currentGPSStatus.latitude, 9));
                  status["gpsLongitude"] = serialized(String(currentGPSStatus.longitude, 9));
                  status["gpsAltitude"] = currentGPSStatus.altitude;
                  status["gpsSiv"] = currentGPSStatus.satellites;
                  status["gpsConnected"] = currentGPSStatus.gpsConnected;
                  status["surveyInActive"] = currentGPSStatus.surveyInActive;
                  status["surveyInObservationTime"] = currentGPSStatus.surveyInObservationTime;
                  status["surveyInValid"] = currentGPSStatus.surveyInValid;
                  status["surveyInMeanAccuracy"] = currentGPSStatus.surveyInMeanAccuracy;
                  status["gpsCurrentTime"] = currentGPSStatus.gpsCurrentTime;
                  status["x"] = currentGPSStatus.x;
                  status["y"] = currentGPSStatus.y;
                  status["z"] = currentGPSStatus.z;
                  status["gpsMode"] = currentGPSStatus.gpsModeString;

                  serializeJson(status, message);
                  request->send(200, "application/json", message);
              });

    server.on("/applySettings", HTTP_GET, [](AsyncWebServerRequest *request) {
      info("Applying settings");
      String inputMessage;
      // Param count
      size_t param_count = request->params();
      for (int i = 0; i < param_count; i++) {
        AsyncWebParameter *h = request->getParam(i);
        debugf("Param %s: %s", h->name().c_str(), h->value().c_str());
        writeSettings(h->name(), h->value());
      }

      delay(100);

      request->send(200, "text/plain", inputMessage);

      ESP.restart();
    });

    server.on("/stopSurvey", HTTP_GET, [](AsyncWebServerRequest *request) {
        stopSurveyMode();  // Implement this function to stop the survey
        info("Survey stopped");
        request->send(200, "text/plain", "Survey stopped");
    });

    server.on("/update", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            // The response will be sent when the upload is complete
        },
        handleUpload
    );

    server.onNotFound(notFound);
}