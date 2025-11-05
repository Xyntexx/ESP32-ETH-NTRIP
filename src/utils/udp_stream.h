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

    // Buffer to accumulate characters before sending
    static const size_t BUFFER_SIZE = 512;
    uint8_t buffer[BUFFER_SIZE];
    size_t bufferPos;

    // Send accumulated buffer contents
    void sendBuffer() {
        if (initialized && bufferPos > 0) {
            udp.broadcastTo(buffer, bufferPos, port);
            bufferPos = 0;
        }
    }

public:
    UDPStream(IPAddress broadcastAddr = IPAddress(255, 255, 255, 255), uint16_t port = 8888)
        : broadcastAddress(broadcastAddr), port(port), initialized(false), bufferPos(0) {}

    bool begin() {
        if (udp.listen(port)) {
            initialized = true;
            bufferPos = 0;
            return true;
        }
        return false;
    }

    // Stream interface implementation
    int available() override { return 0; }  // UDP doesn't support reading in this context
    int read() override { return -1; }      // UDP doesn't support reading in this context
    int peek() override { return -1; }      // UDP doesn't support reading in this context

    // Flush buffer on demand or when newline is written
    void flush() override {
        sendBuffer();
    }

    // Print interface implementation - buffer characters until newline or buffer full
    size_t write(uint8_t c) override {
        if (!initialized) {
            return 0;
        }

        // Add character to buffer
        buffer[bufferPos++] = c;

        // Send if buffer is full or newline encountered
        if (bufferPos >= BUFFER_SIZE || c == '\n') {
            sendBuffer();
        }

        return 1;
    }

    // Write multiple bytes at once - more efficient for bulk writes
    size_t write(const uint8_t *data, size_t size) override {
        if (!initialized) {
            return 0;
        }

        // If data is larger than buffer or buffer would overflow, send buffer first
        if (size >= BUFFER_SIZE || bufferPos + size >= BUFFER_SIZE) {
            sendBuffer();

            // If data is still larger than buffer, send directly
            if (size >= BUFFER_SIZE) {
                udp.broadcastTo(const_cast<uint8_t*>(data), size, port);
                return size;
            }
        }

        // Add to buffer
        memcpy(buffer + bufferPos, data, size);
        bufferPos += size;

        // Check if we need to flush (look for newline in added data)
        for (size_t i = 0; i < size; i++) {
            if (data[i] == '\n') {
                sendBuffer();
                break;
            }
        }

        return size;
    }
}; 