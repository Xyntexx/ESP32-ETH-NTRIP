#ifndef DEFINES_H_
#define DEFINES_H_
#define LOGSerial USBSerial
#define DEBUG_ASYNC_WEBSERVER_PORT     LOGSerial

// Undefine W6100 library defaults before setting custom GPIO pins
#ifdef MISO_GPIO
#undef MISO_GPIO
#endif
#ifdef MOSI_GPIO
#undef MOSI_GPIO
#endif
#ifdef SCK_GPIO
#undef SCK_GPIO
#endif
#ifdef CS_GPIO
#undef CS_GPIO
#endif
#ifdef INT_GPIO
#undef INT_GPIO
#endif

// Custom GPIO pin configuration for this hardware
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

#define FIRMWARE_VERSION "0.42.0"  // Update this when you release new versions
#define BUILD_DATE __DATE__ " " __TIME__

// Task Priorities
#define GPS_STATUS_TASK_PRIORITY configMAX_PRIORITIES - 3 //     = 22
#define GPS_UART_CHECK_TASK_PRIORITY configMAX_PRIORITIES - 2 // = 23
#define NTRIP_TASK_PRIORITY 1 //                                 =  1
#define WEB_SERVER_TASK_PRIORITY 10 //                           = 10
// AsyncTCP task priority                                        =  3
// W6100 task priority (rx)                                      =  1

// Task Stack Sizes (bytes)
#define GPS_STATUS_TASK_STACK 4096      // Stack for GPS status monitoring task
#define GPS_UART_CHECK_TASK_STACK 2048  // Stack for GPS UART check task
#define NTRIP_TASK_STACK 8192           // Stack for NTRIP client task
#define WEB_SERVER_TASK_STACK 8192      // Stack for web server task

// Timeout Constants (milliseconds)
#define WATCHDOG_TIMEOUT_MS 30000       // Watchdog timer timeout (30 seconds)
#define DHCP_TIMEOUT_MS 30000           // DHCP initialization timeout (30 seconds)
#define NTRIP_CONNECTION_TIMEOUT_MS 10000    // NTRIP server connection timeout (10 seconds)
#define NTRIP_RECONNECT_DELAY_MS 5000        // Delay between NTRIP reconnection attempts (5 seconds)
#define NTRIP_STABILITY_TIMEOUT_MS 5000      // Time before NTRIP connection considered stable (5 seconds)
#define NTRIP_RTCM_TIMEOUT_MS 10000          // Timeout for receiving RTCM data (10 seconds)
#define NTRIP_HEALTH_CHECK_INTERVAL_MS 5000  // Interval for NTRIP health checks (5 seconds)
#define UPTIME_PRINT_INTERVAL_MS 300000      // System uptime print interval (5 minutes)

// GPS Constants
#define GPS_SELECTED_BAUD 460800        // Selected baud rate for GPS communication
#define GPS_BAUD_TEST_COUNT 4           // Number of baud rates to test during initialization

// Buffer Sizes
#define NTRIP_SERVER_BUFFER_SIZE 1024   // Buffer size for NTRIP server requests


#endif // DEFINES_H_
