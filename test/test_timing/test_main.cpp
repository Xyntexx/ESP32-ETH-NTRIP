#include <unity.h>
#include <stdint.h>

// Test millis() overflow handling
// This tests the unsigned arithmetic used for timeout calculations

void setUp(void) {
}

void tearDown(void) {
}

// Simulate the RTCM timeout check logic
bool check_timeout(uint32_t current_millis, uint32_t last_data_millis, uint32_t timeout_ms) {
    // This is the fixed logic from ntrip.cpp
    // Note: unsigned long is 64-bit on Linux but 32-bit on ESP32/Windows
    // Use uint32_t explicitly to match ESP32 behavior
    uint32_t elapsed = current_millis - last_data_millis;
    return elapsed > timeout_ms;
}

// Test normal operation (no overflow)
void test_timeout_normal_operation(void) {
    uint32_t last_data = 1000;
    uint32_t current = 11000;
    uint32_t timeout = 10000;

    // At exactly timeout (10000ms), should NOT be timed out (uses > not >=)
    bool timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_FALSE(timed_out); // 10000ms elapsed, not timed out yet

    // Just over timeout (10001ms), should be timed out
    current = 11001;
    timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_TRUE(timed_out); // 10001ms elapsed, timed out
}

// Test at millis() overflow boundary
void test_timeout_at_overflow(void) {
    // millis() overflows at UINT32_MAX (4,294,967,295)
    uint32_t last_data = 0xFFFFFFFE; // Just before overflow (4,294,967,294)
    uint32_t current = 5;             // After overflow (5)
    uint32_t timeout = 10000;

    // Time elapsed: (5 - 4294967294) in unsigned arithmetic = 7
    bool timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_FALSE(timed_out); // Only 7ms elapsed
}

// Test long timeout across overflow
void test_timeout_long_duration_across_overflow(void) {
    // Last data received just before overflow
    uint32_t last_data = 0xFFFFFFFF - 5000; // 5000ms before overflow
    uint32_t current = 6000;                  // 6000ms after overflow
    uint32_t timeout = 10000;

    // Total elapsed: 5000 + 6000 = 11000ms
    // In unsigned arithmetic: (6000 - (4294967295 - 5000)) = 11000
    bool timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_TRUE(timed_out); // 11000ms elapsed, should time out
}

// Test edge case: exactly at timeout
void test_timeout_exactly_at_limit(void) {
    uint32_t last_data = 1000;
    uint32_t current = 11000;
    uint32_t timeout = 10000;

    bool timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_FALSE(timed_out); // Exactly 10000ms, not yet timed out

    current = 11001;
    timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_TRUE(timed_out); // 10001ms, timed out
}

// Test reconnection delay calculation with overflow
void test_reconnection_delay_overflow(void) {
    uint32_t previous_attempt = 0xFFFFFFF0; // 16ms before overflow
    uint32_t current = 20;                   // 20ms after overflow
    uint32_t reconnect_delay = 5000;

    // Time since last attempt: (20 - 4294967280) = 36ms in unsigned arithmetic
    uint32_t elapsed = current - previous_attempt;
    TEST_ASSERT_EQUAL_UINT32(36, elapsed);

    bool should_reconnect = elapsed >= reconnect_delay;
    TEST_ASSERT_FALSE(should_reconnect); // Only 36ms elapsed, need 5000ms
}

// Test 49 day overflow scenario
void test_49_day_overflow(void) {
    // millis() overflows every 49.7 days (2^32 milliseconds)
    // Test that our logic works correctly at that boundary

    // Scenario: Last data at 49 days - 1 second
    uint32_t last_data = 0xFFFFFFFF - 1000;
    // Current time: 2 seconds after overflow
    uint32_t current = 2000;
    uint32_t timeout = 10000;

    // Elapsed: 1000 + 2000 = 3000ms
    bool timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_FALSE(timed_out); // Only 3000ms elapsed
}

// Test connection stability period overflow
void test_stability_period_overflow(void) {
    // Connection opened just before overflow
    uint32_t connection_opened = 0xFFFFFFFF - 2000; // 2 seconds before overflow
    uint32_t current = 4000;                         // 4 seconds after overflow
    uint32_t stability_timeout = 5000;               // 5 second grace period

    // Elapsed: 2000 + 4000 = 6000ms
    uint32_t elapsed = current - connection_opened;
    bool in_stability_period = elapsed < stability_timeout;

    TEST_ASSERT_FALSE(in_stability_period); // 6000ms > 5000ms, out of stability period
}

// Test multiple overflows (theoretical, but tests arithmetic)
void test_multiple_overflows(void) {
    // Even if we somehow had multiple overflows, the arithmetic should work
    uint32_t last_data = 100;
    uint32_t current = 50; // This looks like it went backwards

    // But if we treat it as overflow: (50 - 100) in unsigned = 4294967246
    uint32_t elapsed = current - last_data;

    // This would indicate a VERY long time has passed (invalid scenario in practice)
    // But our logic should handle it gracefully
    TEST_ASSERT_GREATER_THAN_UINT32(1000000, elapsed);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_timeout_normal_operation);
    RUN_TEST(test_timeout_at_overflow);
    RUN_TEST(test_timeout_long_duration_across_overflow);
    RUN_TEST(test_timeout_exactly_at_limit);
    RUN_TEST(test_reconnection_delay_overflow);
    RUN_TEST(test_49_day_overflow);
    RUN_TEST(test_stability_period_overflow);
    RUN_TEST(test_multiple_overflows);

    return UNITY_END();
}
