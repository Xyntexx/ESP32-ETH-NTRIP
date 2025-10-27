#include <unity.h>
#include <string.h>

// Base64 encoding table (same as in ntrip.cpp)
const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Simple String class for testing (mock Arduino String)
class String {
public:
    char* data;
    int len;

    String() : data(nullptr), len(0) {}

    // Constructor from C-string
    String(const char* str) {
        len = strlen(str);
        data = new char[len + 1];
        strcpy(data, str);
    }

    // Constructor from buffer with explicit length (for binary data with null bytes)
    String(const char* buffer, int length) {
        len = length;
        data = new char[len + 1];
        memcpy(data, buffer, len);
        data[len] = '\0';
    }

    ~String() { if (data) delete[] data; }

    const char* c_str() const { return data ? data : ""; }
    int length() const { return len; }

    String& operator+=(char c) {
        char* new_data = new char[len + 2];
        if (data) {
            memcpy(new_data, data, len);
            delete[] data;
        }
        new_data[len] = c;
        new_data[len + 1] = '\0';
        data = new_data;
        len++;
        return *this;
    }
};

// Base64 encoding function from ntrip.cpp
String base64_encode(const String& input) {
    String result;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    int in_len = input.length();
    const char* bytes_to_encode = input.c_str();

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4; i++) {
                result += base64_table[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++) {
            result += base64_table[char_array_4[j]];
        }

        while((i++ < 3)) {
            result += '=';
        }
    }

    return result;
}

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

// RFC 4648 Test Vectors
void test_base64_empty_string(void) {
    String input("");
    String result = base64_encode(input);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_base64_rfc4648_vector_1(void) {
    // "f" -> "Zg=="
    String input("f");
    String result = base64_encode(input);
    TEST_ASSERT_EQUAL_STRING("Zg==", result.c_str());
}

void test_base64_rfc4648_vector_2(void) {
    // "fo" -> "Zm8="
    String input("fo");
    String result = base64_encode(input);
    TEST_ASSERT_EQUAL_STRING("Zm8=", result.c_str());
}

void test_base64_rfc4648_vector_3(void) {
    // "foo" -> "Zm9v"
    String input("foo");
    String result = base64_encode(input);
    TEST_ASSERT_EQUAL_STRING("Zm9v", result.c_str());
}

void test_base64_rfc4648_vector_4(void) {
    // "foob" -> "Zm9vYg=="
    String input("foob");
    String result = base64_encode(input);
    TEST_ASSERT_EQUAL_STRING("Zm9vYg==", result.c_str());
}

void test_base64_rfc4648_vector_5(void) {
    // "fooba" -> "Zm9vYmE="
    String input("fooba");
    String result = base64_encode(input);
    TEST_ASSERT_EQUAL_STRING("Zm9vYmE=", result.c_str());
}

void test_base64_rfc4648_vector_6(void) {
    // "foobar" -> "Zm9vYmFy"
    String input("foobar");
    String result = base64_encode(input);
    TEST_ASSERT_EQUAL_STRING("Zm9vYmFy", result.c_str());
}

// NTRIP-specific test: username:password encoding
void test_base64_ntrip_auth(void) {
    // Test typical NTRIP authentication
    String input("user:pass123");
    String result = base64_encode(input);
    TEST_ASSERT_EQUAL_STRING("dXNlcjpwYXNzMTIz", result.c_str());
}

void test_base64_special_chars(void) {
    // Test with special characters that might appear in passwords
    String input("admin:p@ssw0rd!");
    String result = base64_encode(input);
    // Verify it encodes without crashing
    TEST_ASSERT_TRUE(result.length() > 0);
}

void test_base64_long_string(void) {
    // Test with longer input
    String input("This is a longer string to test the base64 encoder with multiple 3-byte blocks");
    String result = base64_encode(input);
    // Verify proper length (output should be ~4/3 of input)
    int expected_min_len = (input.length() * 4 / 3);
    TEST_ASSERT_GREATER_OR_EQUAL(expected_min_len, result.length());
}

void test_base64_binary_data(void) {
    // Test with actual binary data including null bytes
    const char binary[] = {0x00, (char)0xFF, (char)0xAA, 0x55, 0x00};
    // Use length-based constructor to handle null bytes properly
    String input(binary, 5);
    String result = base64_encode(input);

    // Expected: base64 of [0x00, 0xFF, 0xAA, 0x55, 0x00]
    // Should produce "AP+qVQA=" (8 characters with padding)
    TEST_ASSERT_EQUAL_INT(8, result.length());
    TEST_ASSERT_EQUAL_STRING("AP+qVQA=", result.c_str());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_base64_empty_string);
    RUN_TEST(test_base64_rfc4648_vector_1);
    RUN_TEST(test_base64_rfc4648_vector_2);
    RUN_TEST(test_base64_rfc4648_vector_3);
    RUN_TEST(test_base64_rfc4648_vector_4);
    RUN_TEST(test_base64_rfc4648_vector_5);
    RUN_TEST(test_base64_rfc4648_vector_6);
    RUN_TEST(test_base64_ntrip_auth);
    RUN_TEST(test_base64_special_chars);
    RUN_TEST(test_base64_long_string);
    RUN_TEST(test_base64_binary_data);

    return UNITY_END();
}
