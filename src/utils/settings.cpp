#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "settings.h"
#include "log.h"

DynamicJsonDocument settings(1024);
DynamicJsonDocument status(1024);

Preferences preferences;

bool readSettings()
{
  preferences.begin("settings", false);
  settings["ntrip_sName"] = preferences.getString("ntrip_sName", "");
  settings["enableCaster1"] = preferences.getBool("enableCaster1", false);
  settings["enableCaster2"] = preferences.getBool("enableCaster2", false);
  settings["casterHost1"] = preferences.getString("casterHost1", "");
  settings["casterHost2"] = preferences.getString("casterHost2", "");
  settings["casterPort1"] = preferences.getUShort("casterPort1", 0);
  settings["casterPort2"] = preferences.getUShort("casterPort2", 0);
  settings["rtk_mntpnt1"] = preferences.getString("rtk_mntpnt1", "");
  settings["rtk_mntpnt2"] = preferences.getString("rtk_mntpnt2", "");
  settings["rtk_mntpnt_pw1"] = preferences.getString("rtk_mntpnt_pw1", "");
  settings["rtk_mntpnt_pw2"] = preferences.getString("rtk_mntpnt_pw2", "");

  // Try new key names first, then fall back to old key names
  String user1 = preferences.getString("user1", "");
  if (user1.isEmpty()) {
    user1 = preferences.getString("rtk_mntpnt_user1", "");
    if (!user1.isEmpty()) {
      // Migrate old key to new key
      preferences.putString("user1", user1);
      preferences.remove("rtk_mntpnt_user1");
    }
  }
  settings["rtk_mntpnt_user1"] = user1;

  String user2 = preferences.getString("user2", "");
  if (user2.isEmpty()) {
    user2 = preferences.getString("rtk_mntpnt_user2", "");
    if (!user2.isEmpty()) {
      // Migrate old key to new key
      preferences.putString("user2", user2);
      preferences.remove("rtk_mntpnt_user2");
    }
  }
  settings["rtk_mntpnt_user2"] = user2;

  settings["rtcmChk"] = preferences.getBool("rtcmChk", true);
  settings["ntripVersion1"] = preferences.getInt("ntripVersion1", 1);  // Default to 1 if not set
  settings["ntripVersion2"] = preferences.getInt("ntripVersion2", 1);  // Default to 1 if not set
  settings["ecefX"] = preferences.getLong64("ecefX", 0);
  settings["ecefY"] = preferences.getLong64("ecefY", 0);
  settings["ecefZ"] = preferences.getLong64("ecefZ", 0);
  preferences.end();

  debug("Retrieved settings:");

  // loop thru JSON document
  JsonObject root = settings.as<JsonObject>();
  for (JsonPair kv : root)
  {
    String value;
    serializeJson(settings[kv.key()], value);
    debugf("\t%s: %s", kv.key().c_str(), value.c_str());
  }

  return true;
}

bool writeSettings(String name, String value)
{
  preferences.begin("settings", false);

  debugf("Writing setting %s: %s", name.c_str(), value.c_str());

  // Add length validation for username fields
  if (name == "rtk_mntpnt_user1" || name == "rtk_mntpnt_user2") {
    if (value.length() > 15) { // Limit username to 15 characters
      errorf("Username too long (max 15 chars): %s", value.c_str());
      value = value.substring(0, 15);
    }
  }

  if (name == "casterPort1" || name == "casterPort2")
  {
    int portInt = value.toInt();
    // Validate port range (1-65535)
    if (portInt < 1 || portInt > 65535) {
      errorf("Invalid port number %d for %s (must be 1-65535), using default 2101", portInt, name.c_str());
      portInt = 2101;
    }
    uint16_t portValue = (uint16_t)portInt;
    debugf("Converting port value to uint16_t: %d", portValue);
    preferences.putUShort(name.c_str(), portValue);
  }
  else if (name == "enableCaster1" || name == "enableCaster2" || name == "rtcmChk")
  {
    bool boolValue = (value == "on" || value == "true" || value == "1");
    debugf("Converting to bool: %d", boolValue);
    const char* key = (name == "rtcmChk") ? "rtcmChk" : name.c_str();
    preferences.putBool(key, boolValue);
  }
  else if (name == "ntripVersion1" || name == "ntripVersion2")
  {
    int version = value.toInt();
    // Validate NTRIP version (only 1 or 2 are valid)
    if (version != 1 && version != 2) {
      errorf("Invalid NTRIP version %d for %s (must be 1 or 2), using default 1", version, name.c_str());
      version = 1;
    }
    debugf("Converting to NTRIP version: %d", version);
    preferences.putInt(name.c_str(), version);
  }
  else if (name == "ecefX" || name == "ecefY" || name == "ecefZ")
  {
    // Convert string to int64_t for ECEF coordinates (0.1mm precision)
    // For large values, we need to handle them as strings and convert to int64_t
    int64_t ecefValue = 0;

    // Check if the value is negative
    bool isNegative = value.startsWith("-");
    String absValue = isNegative ? value.substring(1) : value;

    // Convert each character to a digit and build the int64_t value
    for (unsigned int i = 0; i < absValue.length(); i++) {
      char c = absValue.charAt(i);
      if (c >= '0' && c <= '9') {
        ecefValue = ecefValue * 10 + (c - '0');
      }
    }

    // Apply the sign
    if (isNegative) {
      ecefValue = -ecefValue;
    }

    // Validate ECEF coordinates are within Earth bounds (±7,000,000 meters with 0.1mm precision)
    // Earth's radius is ~6,371km, so ±7,000,000 meters = ±70,000,000,000 in 0.1mm units
    const int64_t MAX_ECEF = 70000000000LL;  // 7,000 km in 0.1mm units
    if (ecefValue < -MAX_ECEF || ecefValue > MAX_ECEF) {
      errorf("ECEF coordinate %lld out of bounds for %s (max ±7,000 km), setting to 0", ecefValue, name.c_str());
      ecefValue = 0;
    }

    debugf("Converting ECEF value to int64_t: %lld", ecefValue);
    preferences.putLong64(name.c_str(), ecefValue);
  }
  else
  {
    // Use shorter key names for username fields
    if (name == "rtk_mntpnt_user1") {
      preferences.putString("user1", value);
      // Remove old key if it exists
      preferences.remove("rtk_mntpnt_user1");
    }
    else if (name == "rtk_mntpnt_user2") {
      preferences.putString("user2", value);
      // Remove old key if it exists
      preferences.remove("rtk_mntpnt_user2");
    }
    else {
      preferences.putString(name.c_str(), value);
    }
  }

  preferences.end();
  return true;
}

bool writeSettings(String name, double value) {
  preferences.begin("settings", false);
  debugf("Writing setting %s: %f", name.c_str(), value);
  preferences.putDouble(name.c_str(), value);
  preferences.end();
  return true;
}

bool writeSettings(String name, int64_t value) {
  preferences.begin("settings", false);
  debugf("Writing setting %s: %lld", name.c_str(), value);
  preferences.putLong64(name.c_str(), value);
  preferences.end();
  return true;
}