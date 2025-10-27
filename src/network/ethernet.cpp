#include "ethernet.h"
#include "core/defines.h"
#include "utils/log.h"
#include "w6100/esp32_sc_w6100.h"

// Ethernet configuration
IPAddress staticIP(STATIC_IP);
IPAddress gateway(STATIC_GW);
IPAddress subnet(STATIC_SN);
IPAddress dns(STATIC_DNS);

void getMacAddress(uint8_t* mac) {
    uint64_t chipmacid = ESP.getEfuseMac();
    mac[5] = (chipmacid >> 40) & 0xFF;
    mac[4] = (chipmacid >> 32) & 0xFF;
    mac[3] = (chipmacid >> 24) & 0xFF;
    mac[2] = (chipmacid >> 16) & 0xFF;
    mac[1] = (chipmacid >> 8) & 0xFF;
    mac[0] = chipmacid & 0xFF;
}

bool initializeEthernet() {
    debug("Initializing Ethernet...");
    ESP32_W6100_onEvent();
    
    // Get the ESP32's MAC address
    uint8_t mac[6];
    getMacAddress(mac);
    
    debugf("MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    ETH.begin(MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST, mac);
    
    // Wait for connection
    if (USE_STATIC) {
        ETH.config(staticIP, gateway, subnet, dns);
    } else {
        debug("Waiting for DHCP connection...");
        while (ETH.localIP() == INADDR_NONE) {
            delay(500);
        }
    }

    if (ETH.localIP() == INADDR_NONE) {
        errorf("Failed to configure network - IP: %s", ETH.localIP().toString().c_str());
        return false;
    }

    debugf("Network configured - IP: %s", ETH.localIP().toString().c_str());
    return true;
}
