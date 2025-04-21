#include "system_status.h"

// Global variable definition
unsigned long startupTime = 0;

String getUptimeString() {
    const unsigned long uptime = millis() - startupTime;
    
    // Calculate time components
    const unsigned long days = uptime / (24UL * 60 * 60 * 1000);
    const unsigned long hours = (uptime % (24UL * 60 * 60 * 1000)) / (60 * 60 * 1000);
    const unsigned long minutes = (uptime % (60 * 60 * 1000)) / (60 * 1000);
    const unsigned long seconds = (uptime % (60 * 1000)) / 1000;

    // Format the string based on the largest time unit
    char uptimeStr[50];
    if (days > 0) {
        snprintf(uptimeStr, sizeof(uptimeStr), "%lud %02luh %02lum %02lus", days, hours, minutes, seconds);
    } else if (hours > 0) {
        snprintf(uptimeStr, sizeof(uptimeStr), "%luh %02lum %02lus", hours, minutes, seconds);
    } else {
        snprintf(uptimeStr, sizeof(uptimeStr), "%lum %02lus", minutes, seconds);
    }
    
    return String(uptimeStr);
} 