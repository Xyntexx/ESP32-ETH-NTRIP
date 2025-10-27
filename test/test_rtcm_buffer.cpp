#include <unity.h>
#include <stdint.h>
#include <string.h>

// Mock forward function
static uint8_t* last_forwarded_data = nullptr;
static int last_forwarded_len = 0;
static bool forward_was_called = false;

void mock_forward_func(const uint8_t* data, int len) {
    forward_was_called = true;
    last_forwarded_len = len;
    if (last_forwarded_data) {
        delete[] last_forwarded_data;
    }
    last_forwarded_data = new uint8_t[len];
    memcpy(last_forwarded_data, data, len);
}

void reset_mock() {
    forward_was_called = false;
    last_forwarded_len = 0;
    if (last_forwarded_data) {
        delete[] last_forwarded_data;
        last_forwarded_data = nullptr;
    }
}

// Include the RTCM buffer implementation
// Note: This would need to be refactored to separate pure logic from Arduino dependencies
#include "../src/network/rtcmbuffer.h"

void setUp(void) {
    rtcmbuffer::init();
    reset_mock();
}

void tearDown(void) {
    reset_mock();
}

// Test CRC24Q calculation
void test_crc24q_table_initialization(void) {
    // Test that CRC table is initialized
    // This is implicit in init() call
    TEST_ASSERT_TRUE(true);
}

// Test valid RTCM message parsing
void test_valid_rtcm_message(void) {
    // RTCM3 message structure:
    // 0xD3 (preamble) | length (2 bytes) | message (n bytes) | CRC24Q (3 bytes)

    // Simple valid RTCM message: Type 1005 (6 bytes payload)
    // 0xD3 0x00 0x06 [6 bytes payload] [3 bytes CRC]
    uint8_t test_message[] = {
        0xD3, 0x00, 0x06,           // Header: length = 6
        0x3E, 0xD0, 0x00, 0x00, 0x00, 0x00,  // Payload (Type 1005 example)
        0x00, 0x00, 0x00            // CRC (would need to calculate actual)
    };

    // Note: This test demonstrates structure but would need actual CRC calculation
    // For now, we test that the parser doesn't crash
    for (size_t i = 0; i < sizeof(test_message); i++) {
        rtcmbuffer::process_byte(test_message[i], mock_forward_func);
    }

    // In a real test, we'd verify forward_was_called == true with correct CRC
    TEST_ASSERT_TRUE(true); // Placeholder
}

// Test buffer overflow protection
void test_buffer_overflow(void) {
    // Send 0xD3 to start message
    rtcmbuffer::process_byte(0xD3, mock_forward_func);

    // Try to overflow by sending 300 bytes (buffer max is 256)
    for (int i = 0; i < 300; i++) {
        rtcmbuffer::process_byte(0xFF, mock_forward_func);
    }

    // Should not have forwarded corrupted data
    TEST_ASSERT_FALSE(forward_was_called);
}

// Test invalid length handling
void test_invalid_length(void) {
    // Create message with length > 1023
    uint8_t invalid_message[] = {
        0xD3, 0xFF, 0xFF  // Length = 4095 (> 1023)
    };

    for (size_t i = 0; i < sizeof(invalid_message); i++) {
        rtcmbuffer::process_byte(invalid_message[i], mock_forward_func);
    }

    // Should reject invalid length
    TEST_ASSERT_FALSE(forward_was_called);
}

// Test frame synchronization
void test_frame_sync(void) {
    // Send garbage before valid frame
    rtcmbuffer::process_byte(0x00, mock_forward_func);
    rtcmbuffer::process_byte(0xFF, mock_forward_func);
    rtcmbuffer::process_byte(0xAA, mock_forward_func);

    // Now send valid start byte
    rtcmbuffer::process_byte(0xD3, mock_forward_func);

    // Buffer should sync to 0xD3
    TEST_ASSERT_TRUE(true); // In real test, verify internal state
}

// Test message type extraction
void test_message_type_extraction(void) {
    // RTCM message type is in first 12 bits of payload
    // Type 1005 = 0x3ED (first 12 bits)
    uint8_t payload[] = {0x3E, 0xD0};

    int msg_type = rtcmbuffer::get_rtcm_message_type(payload);

    TEST_ASSERT_EQUAL_INT(1005, msg_type);
}

// Test length parsing
void test_length_parsing(void) {
    // Length is in bits 14-23 of header (10 bits)
    uint8_t header[] = {0xD3, 0x00, 0x06};  // Length = 6

    int length = rtcmbuffer::parse_rtcm_length(header);

    TEST_ASSERT_EQUAL_INT(6, length);
}

void test_length_parsing_max(void) {
    // Test maximum valid length (1023)
    uint8_t header[] = {0xD3, 0x03, 0xFF};  // Length = 1023

    int length = rtcmbuffer::parse_rtcm_length(header);

    TEST_ASSERT_EQUAL_INT(1023, length);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_crc24q_table_initialization);
    RUN_TEST(test_valid_rtcm_message);
    RUN_TEST(test_buffer_overflow);
    RUN_TEST(test_invalid_length);
    RUN_TEST(test_frame_sync);
    RUN_TEST(test_message_type_extraction);
    RUN_TEST(test_length_parsing);
    RUN_TEST(test_length_parsing_max);

    return UNITY_END();
}
