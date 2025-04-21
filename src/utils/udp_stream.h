#pragma once

#include <Arduino.h>
#include <AsyncUDP.h>
#include <Stream.h>

class UDPStream : public Stream {
private:
    AsyncUDP udp;
    IPAddress broadcastAddress;
    uint16_t port;
    bool initialized;

public:
    UDPStream(IPAddress broadcastAddr = IPAddress(255, 255, 255, 255), uint16_t port = 8888)
        : broadcastAddress(broadcastAddr), port(port), initialized(false) {}

    bool begin() {
        if (udp.listen(port)) {
            initialized = true;
            return true;
        }
        return false;
    }

    // Stream interface implementation
    int available() override { return 0; }  // UDP doesn't support reading in this context
    int read() override { return -1; }      // UDP doesn't support reading in this context
    int peek() override { return -1; }      // UDP doesn't support reading in this context
    void flush() override {}                // No-op for UDP

    // Print interface implementation
    size_t write(uint8_t c) override {
        if (initialized) {
            uint8_t buffer[1] = {c};
            udp.broadcastTo(buffer, 1, port);
            return 1;
        }
        return 0;
    }

    size_t write(uint8_t *buffer, size_t size) {
        if (initialized) {
            udp.broadcastTo(buffer, size, port);
            return size;
        }
        return 0;
    }
}; 