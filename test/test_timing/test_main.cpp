#include <unity.h>
#include <stdint.h>

// Test millis() overflow handling
// This tests the unsigned arithmetic used for timeout calculations

void setUp(void) {
}

void tearDown(void) {
}

// Simulate the RTCM timeout check logic
bool check_timeout(unsigned long current_millis, unsigned long last_data_millis, unsigned long timeout_ms) {
    // This is the fixed logic from ntrip.cpp
    return (unsigned long)(current_millis - last_data_millis) > timeout_ms;
}

// Test normal operation (no overflow)
void test_timeout_normal_operation(void) {
    unsigned long last_data = 1000;
    unsigned long current = 11000;
    unsigned long timeout = 10000;

    bool timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_FALSE(timed_out); // 10000ms elapsed, not timed out yet

    current = 11001;
    timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_TRUE(timed_out); // 10001ms elapsed, timed out
}

// Test at millis() overflow boundary
void test_timeout_at_overflow(void) {
    // millis() overflows at UINT32_MAX (4,294,967,295)
    unsigned long last_data = 0xFFFFFFFE; // Just before overflow (4,294,967,294)
    unsigned long current = 5;             // After overflow (5)
    unsigned long timeout = 10000;

    // Time elapsed: (5 - 4294967294) in unsigned arithmetic = 7
    bool timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_FALSE(timed_out); // Only 7ms elapsed
}

// Test long timeout across overflow
void test_timeout_long_duration_across_overflow(void) {
    // Last data received just before overflow
    unsigned long last_data = 0xFFFFFFFF - 5000; // 5000ms before overflow
    unsigned long current = 6000;                  // 6000ms after overflow
    unsigned long timeout = 10000;

    // Total elapsed: 5000 + 6000 = 11000ms
    // In unsigned arithmetic: (6000 - (4294967295 - 5000)) = 11000
    bool timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_TRUE(timed_out); // 11000ms elapsed, should time out
}

// Test edge case: exactly at timeout
void test_timeout_exactly_at_limit(void) {
    unsigned long last_data = 1000;
    unsigned long current = 11000;
    unsigned long timeout = 10000;

    bool timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_FALSE(timed_out); // Exactly 10000ms, not yet timed out

    current = 11001;
    timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_TRUE(timed_out); // 10001ms, timed out
}

// Test reconnection delay calculation with overflow
void test_reconnection_delay_overflow(void) {
    unsigned long previous_attempt = 0xFFFFFFF0; // 16ms before overflow
    unsigned long current = 20;                   // 20ms after overflow
    unsigned long reconnect_delay = 5000;

    // Time since last attempt: (20 - 4294967280) = 36ms in unsigned arithmetic
    unsigned long elapsed = (unsigned long)(current - previous_attempt);
    TEST_ASSERT_EQUAL_UINT32(36, elapsed);

    bool should_reconnect = elapsed >= reconnect_delay;
    TEST_ASSERT_FALSE(should_reconnect); // Only 36ms elapsed, need 5000ms
}

// Test 49 day overflow scenario
void test_49_day_overflow(void) {
    // millis() overflows every 49.7 days (2^32 milliseconds)
    // Test that our logic works correctly at that boundary

    // Scenario: Last data at 49 days - 1 second
    unsigned long last_data = 0xFFFFFFFF - 1000;
    // Current time: 2 seconds after overflow
    unsigned long current = 2000;
    unsigned long timeout = 10000;

    // Elapsed: 1000 + 2000 = 3000ms
    bool timed_out = check_timeout(current, last_data, timeout);
    TEST_ASSERT_FALSE(timed_out); // Only 3000ms elapsed
}

// Test connection stability period overflow
void test_stability_period_overflow(void) {
    // Connection opened just before overflow
    unsigned long connection_opened = 0xFFFFFFFF - 2000; // 2 seconds before overflow
    unsigned long current = 4000;                         // 4 seconds after overflow
    unsigned long stability_timeout = 5000;               // 5 second grace period

    // Elapsed: 2000 + 4000 = 6000ms
    unsigned long elapsed = (unsigned long)(current - connection_opened);
    bool in_stability_period = elapsed < stability_timeout;

    TEST_ASSERT_FALSE(in_stability_period); // 6000ms > 5000ms, out of stability period
}

// Test multiple overflows (theoretical, but tests arithmetic)
void test_multiple_overflows(void) {
    // Even if we somehow had multiple overflows, the arithmetic should work
    unsigned long last_data = 100;
    unsigned long current = 50; // This looks like it went backwards

    // But if we treat it as overflow: (50 - 100) in unsigned = 4294967246
    unsigned long elapsed = (unsigned long)(current - last_data);

    // This would indicate a VERY long time has passed (invalid scenario in practice)
    // But our logic should handle it gracefully
    TEST_ASSERT_GREATER_THAN(1000000, elapsed);
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
