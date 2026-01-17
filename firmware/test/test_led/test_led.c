#include <unity.h>
#include <ch32v00x.h>
#include "debug.h"

void setUp(void) {
    // This runs before EACH test
}

void tearDown(void) {
    // This runs after EACH test
}

void test_led_pin_mode(void) {
    TEST_MESSAGE("Starting internal calculation check..............");

    // Example test: Check if a register bit is set correctly
    // (This is a simplified logic example)
    TEST_ASSERT_EQUAL(1, 1); 
}


int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_led_pin_mode);
    UNITY_END();

    while(1)
    {

    }
}