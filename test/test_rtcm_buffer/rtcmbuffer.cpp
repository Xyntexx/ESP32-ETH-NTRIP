//
// Created by Markus on 3.5.2025.
//

#include "rtcmbuffer.h"
#include <string.h>  // For memset

#ifndef UNIT_TEST
#include "utils/log.h"
#else
// Mock logging functions for unit tests
#include <stdio.h>
#define error(msg) do {} while(0)
#define errorf(fmt, ...) do {} while(0)
#define debugf(fmt, ...) do {} while(0)
#define debug(msg) do {} while(0)
unsigned long millis() { return 0; }
#endif

#define MAX_RTCM_LEN 1030

namespace rtcmbuffer {

struct time_since {
    int msg_type;
    unsigned long time;
};

time_since time_since_array[10];
int time_since_index = 0;


void print_and_update_time_since(const int msg_type) {
    for (int i = 0; i < time_since_index; i++) {
        if (time_since_array[i].msg_type == msg_type) {
            unsigned long current_time = millis();
            unsigned long elapsed_time = current_time - time_since_array[i].time;
#ifndef UNIT_TEST
            USBSerial.printf("[%lu] RTCM %d interval %lu ms", current_time, msg_type, elapsed_time);
#endif
            time_since_array[i].time = current_time;
            return;
        }
    }
    if (time_since_index < 10) {
        time_since_array[time_since_index].msg_type = msg_type;
        time_since_array[time_since_index].time = millis();
        time_since_index++;
    }
}

// RTCM message max size: 3 header + 1023 payload + 3 CRC = 1029 bytes
#define MAX_BUFFER_LEN 1029

uint8_t rtcm_buffer[MAX_BUFFER_LEN];
int rtcm_index = 0;
int rtcm_length = 0;
bool in_message = false;
uint32_t running_crc = 0;
int msg_type = 0;


// CRC24Q lookup table
uint32_t crc24q_table[256];

void init_crc24q_table() {
    for (int i = 0; i < 256; i++) {
        uint32_t crc = i << 16;
        for (int j = 0; j < 8; j++) {
            crc <<= 1;
            if (crc & 0x1000000)
                crc ^= 0x1864CFB;
        }
        crc24q_table[i] = crc & 0xFFFFFF;
    }
}

void init() {
    init_crc24q_table();
}


void update_crc(uint8_t byte) {
    uint8_t idx = ((running_crc >> 16) ^ byte) & 0xFF;
    running_crc = ((running_crc << 8) ^ crc24q_table[idx]) & 0xFFFFFF;
}

int parse_rtcm_length(uint8_t *buf) {
    return ((buf[1] & 0x03) << 8) | buf[2];
}

int get_rtcm_message_type(const uint8_t *payload) {
    return (payload[0] << 4) | (payload[1] >> 4);
}


void forward_buffer(const uint8_t *data, int len, void (*forward_func)(const uint8_t *, int)) {
    if (len >= 6 && data[0] == 0xD3) {
        msg_type = get_rtcm_message_type(&data[3]);
        debugf("Forwarding RTCM message type %d, length %d", msg_type, len);
        if (forward_func) {
            forward_func(data, len);
        }
    } else {
        debugf("Invalid RTCM message: first byte 0x%02X, length %d", data[0], len);
    }
}

void reset_buffer() {
    rtcm_index = 0;
    rtcm_length = 0;
    in_message = false;
    running_crc = 0;
    memset(rtcm_buffer, 0, sizeof(rtcm_buffer));
}

void process_byte(uint8_t byte, void (*forward_func)(const uint8_t *, int)){
    if (!in_message) {
        if (byte == 0xD3) {
            // Reduced verbosity - don't log every message start
            in_message = true;
            rtcm_index = 0;
            running_crc = 0;
            rtcm_buffer[rtcm_index++] = byte;
        }
        return;
    }

    if (rtcm_index >= MAX_BUFFER_LEN) {
        // Buffer overflow - discard corrupted data, don't forward
        error("RTCM buffer overflow - discarding corrupted data");
        reset_buffer();
        return;
    }

    rtcm_buffer[rtcm_index] = byte;

    if (rtcm_index >= 3 && rtcm_index < rtcm_length + 3) {
        update_crc(byte);  // only update CRC for bytes before last 3
    }

    rtcm_index++;

    if (rtcm_index == 3) {
        rtcm_length = parse_rtcm_length(rtcm_buffer);
        // Only log errors, not every message length
        if (rtcm_length > 1023) {
            error("RTCM length error - discarding message");
            reset_buffer();
            return;
        }
        // Include first 3 header bytes in CRC
        update_crc(rtcm_buffer[0]);
        update_crc(rtcm_buffer[1]);
        update_crc(rtcm_buffer[2]);
    }

    if (rtcm_index == rtcm_length + 6) { // Complete message
        uint32_t expected_crc = (rtcm_buffer[rtcm_index - 3] << 16) |
                                (rtcm_buffer[rtcm_index - 2] << 8) |
                                 rtcm_buffer[rtcm_index - 1];

        if (running_crc == expected_crc) {
            // CRC passed - forward the message (removed verbose logging)
            forward_buffer(rtcm_buffer, rtcm_index, forward_func);
        } else {
            // Only log CRC errors
            errorf("RTCM CRC error: expected 0x%06X, got 0x%06X", expected_crc, running_crc);
        }
        reset_buffer();
    }
}
}
