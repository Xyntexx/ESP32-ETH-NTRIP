#pragma once

#include <Arduino.h>
#include <Stream.h>
#include <vector>

class OutputStream {
private:
    static std::vector<Stream*> streams;
    static bool initialized;

public:
    static void addStream(Stream* s) {
        streams.push_back(s);
        initialized = true;
    }

    static void removeStream(Stream* s) {
        for (auto it = streams.begin(); it != streams.end(); ++it) {
            if (*it == s) {
                streams.erase(it);
                break;
            }
        }
        initialized = !streams.empty();
    }

    static void clearStreams() {
        streams.clear();
        initialized = false;
    }

    static void print(const String& message) {
        if (initialized) {
            for (auto stream : streams) {
                if (stream) {
                    stream->print(message);
                }
            }
        }
    }

    static void println(const String& message) {
        if (initialized) {
            for (auto stream : streams) {
                if (stream) {
                    stream->println(message);
                }
            }
        }
    }

    static void printf(const char* format, ...) {
        if (initialized) {
            char buffer[256];
            va_list args;
            va_start(args, format);
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            for (auto stream : streams) {
                if (stream) {
                    stream->print(buffer);
                }
            }
        }
    }
}; 