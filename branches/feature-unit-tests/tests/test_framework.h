/*
 * test_framework.h - Lightweight Unit Test Framework for MiniRTOS
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Test counters (defined in test_framework.c, linked once) */
extern uint32_t tests_run;
extern uint32_t tests_passed;
extern uint32_t tests_failed;

/* Assertion macros */
#define TEST_ASSERT(cond) \
    do { \
        tests_run++; \
        if (cond) { tests_passed++; } \
        else { \
            tests_failed++; \
            printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual) \
    TEST_ASSERT((expected) == (actual))

#define TEST_ASSERT_NOT_EQUAL(expected, actual) \
    TEST_ASSERT((expected) != (actual))

#define TEST_ASSERT_NOT_NULL(ptr) \
    TEST_ASSERT((ptr) != NULL)

#define TEST_ASSERT_NULL(ptr) \
    TEST_ASSERT((ptr) == NULL)

/* Test case registration */
#define TEST_CASE(name) static void name(void)

#define RUN_TEST(name) \
    do { \
        printf("  [RUN ] %s\n", #name); \
        name(); \
    } while(0)

#endif /* TEST_FRAMEWORK_H */
