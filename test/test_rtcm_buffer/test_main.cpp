#include <unity.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>

// ----- Mock forward function -----
struct ForwardCall {
    std::vector<uint8_t> data;
};

static std::vector<ForwardCall> forwarded;

void mock_forward_func(const uint8_t* data, int len) {
    ForwardCall c;
    c.data.assign(data, data + len);
    forwarded.push_back(c);
}

void reset_mock() {
    forwarded.clear();
}

#include "rtcmbuffer.h"

// ----- CRC24Q reference (independent of buffer module) -----
// Polynomial 0x1864CFB. Same algorithm as production, used here to build
// known-good messages for tests. Keeping a separate implementation avoids
// the test trivially asserting "module agrees with itself".
static uint32_t crc24q(const uint8_t* data, int len) {
    uint32_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= ((uint32_t)data[i]) << 16;
        for (int b = 0; b < 8; b++) {
            crc <<= 1;
            if (crc & 0x1000000) crc ^= 0x1864CFB;
        }
    }
    return crc & 0xFFFFFF;
}

// Build a valid RTCM3 frame: 0xD3 | length(10 bits) | payload | CRC24Q.
// payload_len is the RTCM "Message length" field (max 1023).
// The first 12 bits of payload encode the message type.
static std::vector<uint8_t> build_rtcm(int msg_type, const uint8_t* payload_tail, int payload_len) {
    TEST_ASSERT_TRUE_MESSAGE(payload_len >= 2 && payload_len <= 1023, "payload_len out of range");
    std::vector<uint8_t> frame;
    frame.resize(3 + payload_len + 3);
    frame[0] = 0xD3;
    frame[1] = (payload_len >> 8) & 0x03;
    frame[2] = payload_len & 0xFF;
    // Message type: 12 bits, MSB-first, into first 2 payload bytes.
    frame[3] = (msg_type >> 4) & 0xFF;
    frame[4] = ((msg_type & 0x0F) << 4);
    // Remaining payload bytes filled from caller (or zero-filled).
    for (int i = 2; i < payload_len; i++) {
        frame[3 + i] = payload_tail ? payload_tail[i - 2] : 0x00;
    }
    uint32_t crc = crc24q(frame.data(), 3 + payload_len);
    frame[3 + payload_len + 0] = (crc >> 16) & 0xFF;
    frame[3 + payload_len + 1] = (crc >>  8) & 0xFF;
    frame[3 + payload_len + 2] = (crc >>  0) & 0xFF;
    return frame;
}

static void feed(const std::vector<uint8_t>& bytes) {
    for (uint8_t b : bytes) rtcmbuffer::process_byte(b, mock_forward_func);
}

void setUp(void) {
    rtcmbuffer::init();
    reset_mock();
}

void tearDown(void) {
    reset_mock();
}

// ===== Existing exposed-helper tests =====

void test_message_type_extraction(void) {
    uint8_t payload[] = {0x3E, 0xD0};  // Type 1005
    TEST_ASSERT_EQUAL_INT(1005, rtcmbuffer::get_rtcm_message_type(payload));
}

void test_length_parsing(void) {
    uint8_t header[] = {0xD3, 0x00, 0x06};
    TEST_ASSERT_EQUAL_INT(6, rtcmbuffer::parse_rtcm_length(header));
}

void test_length_parsing_max(void) {
    uint8_t header[] = {0xD3, 0x03, 0xFF};
    TEST_ASSERT_EQUAL_INT(1023, rtcmbuffer::parse_rtcm_length(header));
}

// ===== High-value tests =====

// (3) A valid RTCM message must actually be forwarded with matching content.
void test_valid_rtcm_message_forwards(void) {
    auto frame = build_rtcm(1005, nullptr, 6);
    feed(frame);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, (int)forwarded.size(), "expected one forwarded message");
    TEST_ASSERT_EQUAL_INT((int)frame.size(), (int)forwarded[0].data.size());
    TEST_ASSERT_EQUAL_MEMORY(frame.data(), forwarded[0].data.data(), frame.size());
}

// (1) Spec-max sized message (length=1023, total 1029 bytes) must be forwarded.
// This is the regression test for the v0.42.0 MAX_BUFFER_LEN=256 bug.
void test_max_size_message_forwarded(void) {
    auto frame = build_rtcm(1077, nullptr, 1023);  // 1077 = MSM7 GPS
    TEST_ASSERT_EQUAL_INT(1029, (int)frame.size());
    feed(frame);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, (int)forwarded.size(), "max-size RTCM message was dropped");
    TEST_ASSERT_EQUAL_INT(1029, (int)forwarded[0].data.size());
}

// (2) Boundary sizes around old buffer limit and toward spec max.
void test_message_at_buffer_boundaries(void) {
    const int sizes[] = {6, 250, 256, 257, 512, 1022, 1023};
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        reset_mock();
        // Each successful frame ends with reset_buffer(), so internal state
        // is already idle between iterations.
        auto frame = build_rtcm(1004, nullptr, sizes[i]);
        feed(frame);
        char msg[64];
        snprintf(msg, sizeof(msg), "size=%d not forwarded", sizes[i]);
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, (int)forwarded.size(), msg);
        TEST_ASSERT_EQUAL_INT((int)frame.size(), (int)forwarded[0].data.size());
    }
}

// (4) CRC mismatch must NOT forward.
void test_crc_mismatch_not_forwarded(void) {
    auto frame = build_rtcm(1005, nullptr, 6);
    // Corrupt the last CRC byte.
    frame.back() ^= 0xFF;
    feed(frame);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)forwarded.size(), "frame with bad CRC was forwarded");
}

// (5) After buffer overflow / reset, a subsequent valid message still parses.
void test_resync_after_overflow(void) {
    // Start a message but never finish it; then send junk to force overflow.
    rtcmbuffer::process_byte(0xD3, mock_forward_func);
    for (int i = 0; i < 1100; i++) {
        rtcmbuffer::process_byte(0xAA, mock_forward_func);
    }
    TEST_ASSERT_EQUAL_INT(0, (int)forwarded.size());

    // Now send a clean valid message.
    auto frame = build_rtcm(1005, nullptr, 6);
    feed(frame);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, (int)forwarded.size(), "buffer did not resync after overflow");
}

// (6) Two valid messages back-to-back must both be forwarded.
void test_back_to_back_messages(void) {
    auto a = build_rtcm(1005, nullptr, 6);
    auto b = build_rtcm(1077, nullptr, 200);
    feed(a);
    feed(b);
    TEST_ASSERT_EQUAL_INT(2, (int)forwarded.size());
    TEST_ASSERT_EQUAL_INT((int)a.size(), (int)forwarded[0].data.size());
    TEST_ASSERT_EQUAL_INT((int)b.size(), (int)forwarded[1].data.size());
}

// (7) RTCM 1230 filtering: short (<=10 byte) messages dropped, longer ones pass.
void test_rtcm_1230_short_filtered(void) {
    // payload_len=2 -> total frame = 3 + 2 + 3 = 8 bytes, <= 10 so filtered.
    auto frame = build_rtcm(1230, nullptr, 2);
    TEST_ASSERT_TRUE(frame.size() <= 10);
    feed(frame);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)forwarded.size(), "short 1230 should be filtered");
}

void test_rtcm_1230_long_forwarded(void) {
    // payload_len=8 -> total = 14 bytes, > 10 so should pass.
    auto frame = build_rtcm(1230, nullptr, 8);
    TEST_ASSERT_TRUE(frame.size() > 10);
    feed(frame);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, (int)forwarded.size(), "long 1230 should be forwarded");
}

// ===== Existing structural tests =====

void test_buffer_overflow_protection(void) {
    rtcmbuffer::process_byte(0xD3, mock_forward_func);
    for (int i = 0; i < 1100; i++) {
        rtcmbuffer::process_byte(0xFF, mock_forward_func);
    }
    TEST_ASSERT_EQUAL_INT(0, (int)forwarded.size());
}

void test_invalid_length_rejected(void) {
    uint8_t bad[] = {0xD3, 0xFF, 0xFF};  // 4095 > 1023
    for (auto b : bad) rtcmbuffer::process_byte(b, mock_forward_func);
    TEST_ASSERT_EQUAL_INT(0, (int)forwarded.size());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_message_type_extraction);
    RUN_TEST(test_length_parsing);
    RUN_TEST(test_length_parsing_max);

    RUN_TEST(test_valid_rtcm_message_forwards);
    RUN_TEST(test_max_size_message_forwarded);
    RUN_TEST(test_message_at_buffer_boundaries);
    RUN_TEST(test_crc_mismatch_not_forwarded);
    RUN_TEST(test_resync_after_overflow);
    RUN_TEST(test_back_to_back_messages);
    RUN_TEST(test_rtcm_1230_short_filtered);
    RUN_TEST(test_rtcm_1230_long_forwarded);

    RUN_TEST(test_buffer_overflow_protection);
    RUN_TEST(test_invalid_length_rejected);

    return UNITY_END();
}
