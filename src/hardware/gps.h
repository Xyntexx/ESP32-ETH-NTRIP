#pragma once

#include <Arduino.h>
#include "utils/settings.h"
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>

enum class GPSMode: int8_t {
  UNKNOWN = -1,
  ROVER = 0,
  SURVEY_IN = 1,
  FIXED = 2,
};

// Define the GPSStatus struct
struct GPSStatusStruct {
  const char *status_message = nullptr;
  char *gpsCurrentTime = nullptr;
  bool gpsConnected = false;
  double latitude = 0.0;
  double longitude = 0.0;
  double altitude = 0.0;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  bool surveyInActive = false;
  bool surveyInValid = false;
  uint16_t surveyInObservationTime = 0; // in seconds
  float surveyInMeanAccuracy = 0.0; // in meters
  uint16_t satellites = 0; // satellites in view
  GPSMode gpsMode = GPSMode::UNKNOWN; // 0: rover, 1: survey-in, 2: static
  const char *gpsModeString = nullptr; // "rover", "survey-in", or "static"
  // Survey parameters
  uint16_t requestedSurveyTime = 0;
  float requestedSurveyAccuracy = 0.0f;
};

extern bool gpsConnected;

extern SFE_UBLOX_GNSS myGNSS;

extern GPSStatusStruct currentGPSStatus; // Declare currentGPSStatus as an external variable

void initializeGPS();
void stopSurveyMode();
String getSurveyStatus();
bool saveSurveyPosition();
