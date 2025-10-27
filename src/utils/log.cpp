#include <Arduino.h>
#include "log.h"
#include "output_stream.h"
#include "udp_stream.h"
#include <stdarg.h>

// Global variable definitions
String logs[LOG_SIZE];
int prevPosition = 0;

// UDP stream instance
static UDPStream* udpStream = nullptr;

// Helper function to get log level string
String getLevelString(LogLevel level) {
    switch(level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

String getLog()
{
  String response = "{\"timestamp\":" + String(millis()) + ",\"log\":[";
  bool firstEntry = true;

  // Build JSON array of logs
  for (int i = 0; i < LOG_SIZE; i++)
  {
    if (logs[i].length() > 0)
    {
      if (!firstEntry)
      {
        response += ",";
      }
      response += logs[i];
      firstEntry = false;
    }
  }

  response += "]}";
  return response;
}

void addToLog(const String& input, LogLevel level)
{
    // Format the message with timestamp and level
    String formattedMessage = "[" + String(millis()) + "][" + getLevelString(level) + "] " + input;

    // Always send to output stream (USBSerial/UDP)
    OutputStream::println(formattedMessage);

    // Only add to web log if level is INFO or higher
    if (level >= LogLevel::INFO) {
        // Create timestamped log entry
        const String msg = "[\"" + String(millis()) + "\",\"" + formattedMessage + "\"]";

        if (prevPosition == LOG_SIZE - 1)
        {
            // Shift all logs one position back
            for (int i = 0; i < LOG_SIZE - 1; i++)
            {
                logs[i] = logs[i + 1];
            }
            logs[LOG_SIZE - 1] = msg;
        }
        else
        {
            // Find first empty position
            for (int i = 0; i < LOG_SIZE; i++)
            {
                if (logs[i].length() == 0)
                {
                    logs[i] = msg;
                    prevPosition = i;
                    break;
                }
            }
        }
    }
}

// Convenience functions for different log levels
void debug(const String& message) {
    addToLog(message, LogLevel::DEBUG);
}

void info(const String& message) {
    addToLog(message, LogLevel::INFO);
}

void warning(const String& message) {
    addToLog(message, LogLevel::WARNING);
}

void error(const String& message) {
    addToLog(message, LogLevel::ERROR);
}

// Formatted logging functions
void debugf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    debug(String(buffer));
}

void infof(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    info(String(buffer));
}

void warningf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    warning(String(buffer));
}

void errorf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    error(String(buffer));
}

// Initialization functions
bool initLogging() {
    // Clear any existing streams
    OutputStream::clearStreams();

    // Start USBSerial
    USBSerial.begin(115200);
    delay(1000);
    OutputStream::addStream(&USBSerial);
    debug("USBSerial logging initialized");
    return true;
}

bool initUDPLogging(uint16_t udpPort) {
    // Initialize basic logging first
    if (!initLogging()) {
        return false;
    }

    // Create and initialize UDP stream
    udpStream = new UDPStream(IPAddress(255, 255, 255, 255), udpPort);
    if (udpStream->begin()) {
        OutputStream::addStream(udpStream);
        debug("UDP broadcast logging initialized on port " + String(udpPort));
        return true;
    } else {
        error("Failed to initialize UDP broadcast logging");
        delete udpStream;
        udpStream = nullptr;
        return false;
    }
}