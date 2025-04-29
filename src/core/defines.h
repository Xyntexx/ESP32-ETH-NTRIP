#ifndef DEFINES_H_
#define DEFINES_H_
#define LOGSerial USBSerial
#define DEBUG_ASYNC_WEBSERVER_PORT     LOGSerial

#define MISO_GPIO           6
#define MOSI_GPIO           7
#define SCK_GPIO            5
#define CS_GPIO             10
#define INT_GPIO            3

// Static IP configuration (fallback)
#define USE_STATIC false
#define STATIC_IP 192, 168, 5, 100
#define STATIC_GW 192, 168, 5, 1
#define STATIC_SN 255, 255, 255, 0
#define STATIC_DNS 8, 8, 8, 8

#define FIRMWARE_VERSION "0.39.1"  // Update this when you release new versions
#define BUILD_DATE __DATE__ " " __TIME__

#endif // DEFINES_H_
