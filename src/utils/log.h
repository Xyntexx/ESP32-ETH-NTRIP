#pragma once

#include <Arduino.h>

// Constants
constexpr int LOG_SIZE = 20;

// Logging levels
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

// Global variable declaration
extern String logs[LOG_SIZE];

// Function declarations
String getLog();
void addToLog(const String& input, LogLevel level = LogLevel::INFO);
void debug(const String& message);
void info(const String& message);
void warning(const String& message);
void error(const String& message);

// Formatted logging functions
void debugf(const char* format, ...);
void infof(const char* format, ...);
void warningf(const char* format, ...);
void errorf(const char* format, ...);

// Initialization functions
bool initLogging();
bool initUDPLogging(uint16_t udpPort = 8888);