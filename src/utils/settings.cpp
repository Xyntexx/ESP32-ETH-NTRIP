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
  settings["rtcmChk"] = preferences.getBool("rtcmChk", true);
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

  if (name == "casterPort1" || name == "casterPort2")
  {
    uint16_t portValue = (uint16_t)value.toInt();
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
    
    debugf("Converting ECEF value to int64_t: %lld", ecefValue);
    preferences.putLong64(name.c_str(), ecefValue);
  }
  else
  {
    preferences.putString(name.c_str(), value);
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