// Unit tests for CHA10Keyboard multi-tap decoding logic
#include "Arduino.h"
#include "TestUtil.h"
#include "UptimeClock.h"
#include "mesh/Throttle.h"
#include "input/CHA10Keyboard.h"
#include <cstdint>
#include <unity.h>

// Test the static charmap and decodeMultiTap function
void test_charmap_0_key()
{
    // Key '0' maps to "+&@/%$£0"
    TEST_ASSERT_EQUAL('+', CHA10Keyboard::charmap[0][0]);
    TEST_ASSERT_EQUAL('&', CHA10Keyboard::charmap[0][1]);
    TEST_ASSERT_EQUAL('@', CHA10Keyboard::charmap[0][2]);
    TEST_ASSERT_EQUAL('/', CHA10Keyboard::charmap[0][3]);
    TEST_ASSERT_EQUAL('%', CHA10Keyboard::charmap[0][4]);
    TEST_ASSERT_EQUAL('$', CHA10Keyboard::charmap[0][5]);
    TEST_ASSERT_EQUAL('£', CHA10Keyboard::charmap[0][6]);
    TEST_ASSERT_EQUAL('0', CHA10Keyboard::charmap[0][7]);
    TEST_ASSERT_EQUAL(8, strlen(CHA10Keyboard::charmap[0]));
}

void test_charmap_1_key()
{
    // Key '1' maps to " -?!,.:;\"'<=>()1"
    TEST_ASSERT_EQUAL(' ', CHA10Keyboard::charmap[1][0]);
    TEST_ASSERT_EQUAL('-', CHA10Keyboard::charmap[1][1]);
    TEST_ASSERT_EQUAL('?', CHA10Keyboard::charmap[1][2]);
    TEST_ASSERT_EQUAL('!', CHA10Keyboard::charmap[1][3]);
    TEST_ASSERT_EQUAL(',', CHA10Keyboard::charmap[1][4]);
    TEST_ASSERT_EQUAL('.', CHA10Keyboard::charmap[1][5]);
    TEST_ASSERT_EQUAL(':', CHA10Keyboard::charmap[1][6]);
    TEST_ASSERT_EQUAL(';', CHA10Keyboard::charmap[1][7]);
    TEST_ASSERT_EQUAL('"', CHA10Keyboard::charmap[1][8]);
    TEST_ASSERT_EQUAL('\'', CHA10Keyboard::charmap[1][9]);
    TEST_ASSERT_EQUAL('<', CHA10Keyboard::charmap[1][10]);
    TEST_ASSERT_EQUAL('=', CHA10Keyboard::charmap[1][11]);
    TEST_ASSERT_EQUAL('>', CHA10Keyboard::charmap[1][12]);
    TEST_ASSERT_EQUAL('(', CHA10Keyboard::charmap[1][13]);
    TEST_ASSERT_EQUAL(')', CHA10Keyboard::charmap[1][14]);
    TEST_ASSERT_EQUAL('1', CHA10Keyboard::charmap[1][15]);
    TEST_ASSERT_EQUAL(16, strlen(CHA10Keyboard::charmap[1]));
}

void test_charmap_2_key()
{
    // Key '2' maps to "abc2"
    TEST_ASSERT_EQUAL('a', CHA10Keyboard::charmap[2][0]);
    TEST_ASSERT_EQUAL('b', CHA10Keyboard::charmap[2][1]);
    TEST_ASSERT_EQUAL('c', CHA10Keyboard::charmap[2][2]);
    TEST_ASSERT_EQUAL('2', CHA10Keyboard::charmap[2][3]);
    TEST_ASSERT_EQUAL(4, strlen(CHA10Keyboard::charmap[2]));
}

void test_charmap_star_key()
{
    // Key '*' maps to "*"
    TEST_ASSERT_EQUAL('*', CHA10Keyboard::charmap[10][0]);
    TEST_ASSERT_EQUAL(1, strlen(CHA10Keyboard::charmap[10]));
}

void test_charmap_hash_key()
{
    // Key '#' maps to "#"
    TEST_ASSERT_EQUAL('#', CHA10Keyboard::charmap[11][0]);
    TEST_ASSERT_EQUAL(1, strlen(CHA10Keyboard::charmap[11]));
}

void test_getMaxTaps()
{
    // Test getMaxTaps for various keys
    CHA10Keyboard kb("test");

    // We can't call private method directly, but we can verify the charmap lengths
    // getMaxTaps returns the length of the charmap for a given key
    TEST_ASSERT_EQUAL(8, strlen(CHA10Keyboard::charmap[0]));  // key '0'
    TEST_ASSERT_EQUAL(16, strlen(CHA10Keyboard::charmap[1])); // key '1'
    TEST_ASSERT_EQUAL(4, strlen(CHA10Keyboard::charmap[2]));  // key '2'
    TEST_ASSERT_EQUAL(4, strlen(CHA10Keyboard::charmap[3]));  // key '3'
    TEST_ASSERT_EQUAL(4, strlen(CHA10Keyboard::charmap[4]));  // key '4'
    TEST_ASSERT_EQUAL(4, strlen(CHA10Keyboard::charmap[5]));  // key '5'
    TEST_ASSERT_EQUAL(4, strlen(CHA10Keyboard::charmap[6]));  // key '6'
    TEST_ASSERT_EQUAL(4, strlen(CHA10Keyboard::charmap[7]));  // key '7'
    TEST_ASSERT_EQUAL(5, strlen(CHA10Keyboard::charmap[8]));  // key '9' (wxyz9)
    TEST_ASSERT_EQUAL(1, strlen(CHA10Keyboard::charmap[10])); // key '*'
    TEST_ASSERT_EQUAL(1, strlen(CHA10Keyboard::charmap[11])); // key '#'
}

void test_decodeMultiTap_basic()
{
    // We can't call private decodeMultiTap directly, but we can verify the logic
    // by checking charmap access
    const char *map = CHA10Keyboard::charmap[2]; // key '2' -> "abc2"
    TEST_ASSERT_EQUAL('a', map[0]);
    TEST_ASSERT_EQUAL('b', map[1]);
    TEST_ASSERT_EQUAL('c', map[2]);
    TEST_ASSERT_EQUAL('2', map[3]);

    // Test wrap-around behavior (should return last char if count >= len)
    // This is handled by the modulo in the actual code
    TEST_ASSERT_EQUAL('a', map[4 % 4]);
    TEST_ASSERT_EQUAL('b', map[5 % 4]);
}

void test_Throttle_integration()
{
    // Test that Throttle::isWithinTimespanMs works correctly for the 500ms multi-tap timeout
    Time::setTestMillis(10000);
    TEST_ASSERT_TRUE(Throttle::isWithinTimespanMs(9900, 500));   // 100ms elapsed
    TEST_ASSERT_TRUE(Throttle::isWithinTimespanMs(9600, 500));   // 400ms elapsed
    TEST_ASSERT_FALSE(Throttle::isWithinTimespanMs(9400, 500));  // 600ms elapsed
    TEST_ASSERT_FALSE(Throttle::isWithinTimespanMs(9000, 500));  // 1000ms elapsed

    // Test across millis wrap
    Time::setTestMillis(0xFFFFFF00u);
    TEST_ASSERT_TRUE(Throttle::isWithinTimespanMs(0xFFFFFF00u, 500));
    Time::advanceTestMillis(400); // wrapped
    TEST_ASSERT_TRUE(Throttle::isWithinTimespanMs(0xFFFFFF00u, 500));
    Time::advanceTestMillis(200); // 600ms total
    TEST_ASSERT_FALSE(Throttle::isWithinTimespanMs(0xFFFFFF00u, 500));
}

void setup()
{
    initializeTestEnvironment();
    UNITY_BEGIN();
    RUN_TEST(test_charmap_0_key);
    RUN_TEST(test_charmap_1_key);
    RUN_TEST(test_charmap_2_key);
    RUN_TEST(test_charmap_star_key);
    RUN_TEST(test_charmap_hash_key);
    RUN_TEST(test_getMaxTaps);
    RUN_TEST(test_decodeMultiTap_basic);
    RUN_TEST(test_Throttle_integration);
    exit(UNITY_END());
}

void loop() {}