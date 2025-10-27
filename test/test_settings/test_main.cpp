#include <unity.h>
#include <map>
#include <string>
#include <cstdint>
#include <cstring>

// Mock String class for native testing
class String {
private:
    std::string data;
public:
    String() : data("") {}
    String(const char* str) : data(str ? str : "") {}
    String(const std::string& str) : data(str) {}
    String(int num) : data(std::to_string(num)) {}

    const char* c_str() const { return data.c_str(); }
    size_t length() const { return data.length(); }
    bool startsWith(const char* prefix) const {
        return data.find(prefix) == 0;
    }
    String substring(size_t start) const {
        return String(data.substr(start).c_str());
    }
    String substring(size_t start, size_t end) const {
        return String(data.substr(start, end - start).c_str());
    }
    char charAt(size_t index) const {
        return index < data.length() ? data[index] : 0;
    }
    int toInt() const {
        return std::atoi(data.c_str());
    }
    bool operator==(const char* other) const {
        return data == other;
    }
    bool operator<(const String& other) const {
        return data < other.data;
    }
};

// Mock Preferences class for testing
class Preferences {
private:
    struct Setting {
        enum Type { STRING, INT, UINT16, INT64, BOOL, DOUBLE } type;
        String strValue;
        int intValue;
        uint16_t uint16Value;
        int64_t int64Value;
        bool boolValue;
        double doubleValue;
    };
    std::map<String, Setting> storage;

public:
    bool begin(const char* name, bool readOnly) { return true; }
    void end() {}

    String getString(const char* key, String defaultValue) {
        if (storage.find(key) != storage.end()) return storage[key].strValue;
        return defaultValue;
    }

    int getInt(const char* key, int defaultValue) {
        if (storage.find(key) != storage.end()) return storage[key].intValue;
        return defaultValue;
    }

    uint16_t getUShort(const char* key, uint16_t defaultValue) {
        if (storage.find(key) != storage.end()) return storage[key].uint16Value;
        return defaultValue;
    }

    int64_t getLong64(const char* key, int64_t defaultValue) {
        if (storage.find(key) != storage.end()) return storage[key].int64Value;
        return defaultValue;
    }

    bool getBool(const char* key, bool defaultValue) {
        if (storage.find(key) != storage.end()) return storage[key].boolValue;
        return defaultValue;
    }

    void putString(const char* key, String value) {
        storage[key] = {Setting::STRING, value, 0, 0, 0, false, 0.0};
    }

    void putInt(const char* key, int value) {
        storage[key] = {Setting::INT, "", value, 0, 0, false, 0.0};
    }

    void putUShort(const char* key, uint16_t value) {
        storage[key] = {Setting::UINT16, "", 0, value, 0, false, 0.0};
    }

    void putLong64(const char* key, int64_t value) {
        storage[key] = {Setting::INT64, "", 0, 0, value, false, 0.0};
    }

    void putBool(const char* key, bool value) {
        storage[key] = {Setting::BOOL, "", 0, 0, 0, value, 0.0};
    }

    void putDouble(const char* key, double value) {
        storage[key] = {Setting::DOUBLE, "", 0, 0, 0, false, value};
    }

    void remove(const char* key) {
        storage.erase(key);
    }

    void clear() {
        storage.clear();
    }
};

Preferences preferences;

// Include the actual writeSettings function logic
bool writeSettings(String name, String value) {
    preferences.begin("settings", false);

    // Add length validation for username fields
    if (name == "rtk_mntpnt_user1" || name == "rtk_mntpnt_user2") {
        if (value.length() > 15) {
            value = value.substring(0, 15);
        }
    }

    if (name == "casterPort1" || name == "casterPort2") {
        int portInt = value.toInt();
        // Validate port range (1-65535)
        if (portInt < 1 || portInt > 65535) {
            portInt = 2101;
        }
        uint16_t portValue = (uint16_t)portInt;
        preferences.putUShort(name.c_str(), portValue);
    }
    else if (name == "enableCaster1" || name == "enableCaster2" || name == "rtcmChk") {
        bool boolValue = (value == "on" || value == "true" || value == "1");
        const char* key = (name == "rtcmChk") ? "rtcmChk" : name.c_str();
        preferences.putBool(key, boolValue);
    }
    else if (name == "ntripVersion1" || name == "ntripVersion2") {
        int version = value.toInt();
        // Validate NTRIP version (only 1 or 2 are valid)
        if (version != 1 && version != 2) {
            version = 1;
        }
        preferences.putInt(name.c_str(), version);
    }
    else if (name == "ecefX" || name == "ecefY" || name == "ecefZ") {
        // Convert string to int64_t for ECEF coordinates
        int64_t ecefValue = 0;

        bool isNegative = value.startsWith("-");
        String absValue = isNegative ? value.substring(1) : value;

        for (unsigned int i = 0; i < absValue.length(); i++) {
            char c = absValue.charAt(i);
            if (c >= '0' && c <= '9') {
                ecefValue = ecefValue * 10 + (c - '0');
            }
        }

        if (isNegative) {
            ecefValue = -ecefValue;
        }

        // Validate ECEF coordinates (±7,000 km in 0.1mm units)
        const int64_t MAX_ECEF = 70000000000LL;
        if (ecefValue < -MAX_ECEF || ecefValue > MAX_ECEF) {
            ecefValue = 0;
        }

        preferences.putLong64(name.c_str(), ecefValue);
    }
    else {
        if (name == "rtk_mntpnt_user1") {
            preferences.putString("user1", value);
            preferences.remove("rtk_mntpnt_user1");
        }
        else if (name == "rtk_mntpnt_user2") {
            preferences.putString("user2", value);
            preferences.remove("rtk_mntpnt_user2");
        }
        else {
            preferences.putString(name.c_str(), value);
        }
    }

    preferences.end();
    return true;
}

void setUp(void) {
    preferences.clear();
}

void tearDown(void) {
}

// Test port validation - valid ports
void test_port_validation_valid(void) {
    writeSettings("casterPort1", "2101");

    preferences.begin("settings", false);
    uint16_t port = preferences.getUShort("casterPort1", 0);
    preferences.end();

    TEST_ASSERT_EQUAL_UINT16(2101, port);
}

// Test port validation - port too low
void test_port_validation_too_low(void) {
    writeSettings("casterPort1", "0");

    preferences.begin("settings", false);
    uint16_t port = preferences.getUShort("casterPort1", 0);
    preferences.end();

    TEST_ASSERT_EQUAL_UINT16(2101, port); // Should default to 2101
}

// Test port validation - port too high
void test_port_validation_too_high(void) {
    writeSettings("casterPort1", "65536");

    preferences.begin("settings", false);
    uint16_t port = preferences.getUShort("casterPort1", 0);
    preferences.end();

    TEST_ASSERT_EQUAL_UINT16(2101, port); // Should default to 2101
}

// Test port validation - negative port
void test_port_validation_negative(void) {
    writeSettings("casterPort2", "-100");

    preferences.begin("settings", false);
    uint16_t port = preferences.getUShort("casterPort2", 0);
    preferences.end();

    TEST_ASSERT_EQUAL_UINT16(2101, port); // Should default to 2101
}

// Test NTRIP version validation - valid version 1
void test_ntrip_version_valid_1(void) {
    writeSettings("ntripVersion1", "1");

    preferences.begin("settings", false);
    int version = preferences.getInt("ntripVersion1", 0);
    preferences.end();

    TEST_ASSERT_EQUAL_INT(1, version);
}

// Test NTRIP version validation - valid version 2
void test_ntrip_version_valid_2(void) {
    writeSettings("ntripVersion2", "2");

    preferences.begin("settings", false);
    int version = preferences.getInt("ntripVersion2", 0);
    preferences.end();

    TEST_ASSERT_EQUAL_INT(2, version);
}

// Test NTRIP version validation - invalid version 0
void test_ntrip_version_invalid_0(void) {
    writeSettings("ntripVersion1", "0");

    preferences.begin("settings", false);
    int version = preferences.getInt("ntripVersion1", 0);
    preferences.end();

    TEST_ASSERT_EQUAL_INT(1, version); // Should default to 1
}

// Test NTRIP version validation - invalid version 3
void test_ntrip_version_invalid_3(void) {
    writeSettings("ntripVersion1", "3");

    preferences.begin("settings", false);
    int version = preferences.getInt("ntripVersion1", 0);
    preferences.end();

    TEST_ASSERT_EQUAL_INT(1, version); // Should default to 1
}

// Test ECEF validation - valid coordinate
void test_ecef_validation_valid(void) {
    writeSettings("ecefX", "123456789");

    preferences.begin("settings", false);
    int64_t ecef = preferences.getLong64("ecefX", 0);
    preferences.end();

    TEST_ASSERT_EQUAL_INT64(123456789LL, ecef);
}

// Test ECEF validation - negative coordinate
void test_ecef_validation_negative(void) {
    writeSettings("ecefY", "-987654321");

    preferences.begin("settings", false);
    int64_t ecef = preferences.getLong64("ecefY", 0);
    preferences.end();

    TEST_ASSERT_EQUAL_INT64(-987654321LL, ecef);
}

// Test ECEF validation - out of bounds positive
void test_ecef_validation_out_of_bounds_positive(void) {
    writeSettings("ecefZ", "80000000000"); // Exceeds MAX_ECEF

    preferences.begin("settings", false);
    int64_t ecef = preferences.getLong64("ecefZ", -999);
    preferences.end();

    TEST_ASSERT_EQUAL_INT64(0, ecef); // Should default to 0
}

// Test ECEF validation - out of bounds negative
void test_ecef_validation_out_of_bounds_negative(void) {
    writeSettings("ecefX", "-80000000000"); // Exceeds -MAX_ECEF

    preferences.begin("settings", false);
    int64_t ecef = preferences.getLong64("ecefX", -999);
    preferences.end();

    TEST_ASSERT_EQUAL_INT64(0, ecef); // Should default to 0
}

// Test username length validation - within limit
void test_username_length_valid(void) {
    writeSettings("rtk_mntpnt_user1", "user123");

    preferences.begin("settings", false);
    String username = preferences.getString("user1", "");
    preferences.end();

    TEST_ASSERT_EQUAL_STRING("user123", username.c_str());
}

// Test username length validation - exceeds limit
void test_username_length_exceeds_limit(void) {
    writeSettings("rtk_mntpnt_user2", "verylongusername123456789"); // >15 chars

    preferences.begin("settings", false);
    String username = preferences.getString("user2", "");
    preferences.end();

    TEST_ASSERT_EQUAL_INT(15, username.length());
    TEST_ASSERT_EQUAL_STRING("verylongusernam", username.c_str()); // Truncated to 15
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_port_validation_valid);
    RUN_TEST(test_port_validation_too_low);
    RUN_TEST(test_port_validation_too_high);
    RUN_TEST(test_port_validation_negative);
    RUN_TEST(test_ntrip_version_valid_1);
    RUN_TEST(test_ntrip_version_valid_2);
    RUN_TEST(test_ntrip_version_invalid_0);
    RUN_TEST(test_ntrip_version_invalid_3);
    RUN_TEST(test_ecef_validation_valid);
    RUN_TEST(test_ecef_validation_negative);
    RUN_TEST(test_ecef_validation_out_of_bounds_positive);
    RUN_TEST(test_ecef_validation_out_of_bounds_negative);
    RUN_TEST(test_username_length_valid);
    RUN_TEST(test_username_length_exceeds_limit);

    return UNITY_END();
}
