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

        // Filter out RTCM 1230 (GLONASS biases) when it contains no useful data
        // Message type 1230 with length <= 10 bytes is just header/placeholder without antenna
        if (msg_type == 1230 && len <= 10) {
            debugf("Filtering empty RTCM 1230 (no antenna data), length %d", len);
            return;  // Don't forward this message
        }

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

// RTCM Buffer State Machine - processes incoming bytes from GPS UART
//
// RTCM 3.x Message Format:
// [0xD3] [Length-H] [Length-L] [Payload 0-1023 bytes] [CRC24-H] [CRC24-M] [CRC24-L]
//  byte0   byte1      byte2      byte3...byte(N+2)      byte(N+3) byte(N+4) byte(N+5)
//
// State Machine Flow:
// 1. IDLE (in_message=false): Wait for 0xD3 preamble byte
// 2. HEADER (rtcm_index<3): Collect 3 header bytes to parse length
// 3. PAYLOAD (rtcm_index>=3): Collect N payload bytes, update CRC24
// 4. CRC (rtcm_index==length+6): Verify CRC24, forward if valid
// 5. RESET: Clear buffer and return to IDLE
//
// Length Encoding: 10-bit value split across bytes 1-2
//   byte1: [reserved(2)] [length(8 MSB)]
//   byte2: [length(2 LSB)] [reserved(6)]
//
// CRC24: Computed over header + payload (bytes 0 to N+2), stored in bytes N+3 to N+5
void process_byte(uint8_t byte, void (*forward_func)(const uint8_t *, int)){
    // STATE 1: IDLE - Wait for 0xD3 preamble
    if (!in_message) {
        if (byte == 0xD3) {
            in_message = true;
            rtcm_index = 0;
            running_crc = 0;
            rtcm_buffer[rtcm_index++] = byte;
        }
        return;
    }

    // Safety check: prevent buffer overflow
    if (rtcm_index >= MAX_BUFFER_LEN) {
        error("RTCM buffer overflow - discarding corrupted data");
        reset_buffer();
        return;
    }

    // Store current byte
    rtcm_buffer[rtcm_index] = byte;

    // Update CRC for payload bytes only (not the last 3 CRC bytes)
    if (rtcm_index >= 3 && rtcm_index < rtcm_length + 3) {
        update_crc(byte);
    }

    rtcm_index++;

    // STATE 2: HEADER COMPLETE (after 3 bytes) - Parse message length
    if (rtcm_index == 3) {
        rtcm_length = parse_rtcm_length(rtcm_buffer);

        // Validate length (RTCM 3.x spec: max 1023 bytes payload)
        if (rtcm_length > 1023) {
            error("RTCM length error - discarding message");
            reset_buffer();
            return;
        }

        // Initialize CRC with header bytes
        update_crc(rtcm_buffer[0]);
        update_crc(rtcm_buffer[1]);
        update_crc(rtcm_buffer[2]);
    }

    // STATE 3: MESSAGE COMPLETE - Verify CRC and forward
    // Total message size: 3 (header) + N (payload) + 3 (CRC) = N+6 bytes
    if (rtcm_index == rtcm_length + 6) {
        // Extract 24-bit CRC from last 3 bytes
        uint32_t expected_crc = (rtcm_buffer[rtcm_index - 3] << 16) |
                                (rtcm_buffer[rtcm_index - 2] << 8) |
                                 rtcm_buffer[rtcm_index - 1];

        if (running_crc == expected_crc) {
            // CRC passed - forward valid message to NTRIP caster
            forward_buffer(rtcm_buffer, rtcm_index, forward_func);
        } else {
            errorf("RTCM CRC error: expected 0x%06X, got 0x%06X", expected_crc, running_crc);
        }

        // STATE 4: RESET - Return to IDLE state
        reset_buffer();
    }
}
}
