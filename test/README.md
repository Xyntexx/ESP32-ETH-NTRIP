# ESP32-ETH-NTRIP Unit Tests

This directory contains unit tests for critical components of the NTRIP base station firmware.

## Test Coverage

### 1. RTCM Buffer Processing (`test_rtcm_buffer.cpp`)
Tests the RTCM3 message parser and validator:
- ✓ Valid RTCM message parsing
- ✓ CRC24Q validation
- ✓ Buffer overflow protection
- ✓ Invalid length handling (>1023 bytes)
- ✓ Frame synchronization (0xD3 detection)
- ✓ Message type extraction
- ✓ Length parsing

**Why it matters:** RTCM buffer is critical for data integrity. Bugs here could send corrupted correction data to rovers, causing incorrect positioning.

### 2. Base64 Encoding (`test_base64.cpp`)
Tests the custom Base64 implementation used for NTRIP 2.0 authentication:
- ✓ RFC 4648 test vectors
- ✓ Empty string handling
- ✓ Padding cases (1, 2, 3 byte inputs)
- ✓ NTRIP authentication format (username:password)
- ✓ Special characters
- ✓ Long strings
- ✓ Binary data

**Why it matters:** Incorrect Base64 encoding prevents NTRIP 2.0 authentication, causing connection failures.

### 3. Timing/Overflow Handling (`test_timing.cpp`)
Tests millis() overflow scenarios and timeout calculations:
- ✓ Normal timeout operation
- ✓ Timeout at millis() overflow (49 days)
- ✓ Long timeout across overflow boundary
- ✓ Reconnection delay with overflow
- ✓ Connection stability period
- ✓ Edge cases

**Why it matters:** After 49.7 days of uptime, millis() overflows. Incorrect handling causes false timeouts or hung connections.

## Running Tests

### Run all tests:
```bash
pio test
```

### Run specific test:
```bash
pio test -f test_base64
```

### Run tests on native (fast):
```bash
pio test -e native
```

## Test Architecture

Tests are designed to run on native (PC) platform for fast iteration. They use:
- **Unity Test Framework** (built into PlatformIO)
- **Mocked Arduino functions** where needed
- **Pure logic extraction** from embedded code

## Adding New Tests

1. Create `test/test_<component>.cpp`
2. Include Unity framework: `#include <unity.h>`
3. Implement setUp() and tearDown()
4. Write test functions: `void test_<name>(void)`
5. Use assertions: `TEST_ASSERT_*`
6. Create main() with `UNITY_BEGIN()` and `UNITY_END()`

Example:
```cpp
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_my_function(void) {
    int result = my_function(5);
    TEST_ASSERT_EQUAL_INT(10, result);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_my_function);
    return UNITY_END();
}
```

## Integration with CI/CD

These tests can be integrated into GitHub Actions or other CI systems:

```yaml
- name: Run Tests
  run: pio test -e native
```

## Known Limitations

1. **Hardware dependencies:** Tests requiring actual ESP32 hardware (WiFi, GPS, Ethernet) are not included
2. **Mock complexity:** Some functions are difficult to mock without major refactoring
3. **State management:** FreeRTOS tasks and mutexes can't be tested in native environment

## Future Test Additions

- [ ] Settings validation (username length, port numbers)
- [ ] ECEF coordinate parsing
- [ ] Error message formatting
- [ ] Connection state machine
- [ ] JSON configuration parsing
- [ ] OTA update state tracking

## References

- [PlatformIO Unit Testing](https://docs.platformio.org/en/latest/plus/unit-testing.html)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
- [RFC 4648 - Base64 Encoding](https://tools.ietf.org/html/rfc4648)
- [RTCM3 Standard](https://www.rtcm.org/)
