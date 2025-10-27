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

#define FIRMWARE_VERSION "0.41.2"  // Update this when you release new versions
#define BUILD_DATE __DATE__ " " __TIME__

#define GPS_STATUS_TASK_PRIORITY configMAX_PRIORITIES - 3 //     = 22
#define GPS_UART_CHECK_TASK_PRIORITY configMAX_PRIORITIES - 2 // = 23
#define NTRIP_TASK_PRIORITY 1 //                                 =  1
#define WEB_SERVER_TASK_PRIORITY 10 //                           = 10
// AsyncTCP task priority                                        =  3
// W6100 task priority (rx)                                      =  1


#endif // DEFINES_H_
