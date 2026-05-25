/*
 * test_main.c - Unit test entry point for MiniRTOS Host-side tests
 */

#include <stdio.h>
#include <stdint.h>

/* Test framework counters (defined in test_framework.c) */
extern uint32_t tests_run;
extern uint32_t tests_passed;
extern uint32_t tests_failed;

/* External test suite functions */
extern void test_suite_heap4(void);
extern void test_suite_task(void);
extern void test_suite_scheduler(void);
extern void test_suite_queue(void);
extern void test_suite_semaphore(void);
extern void test_suite_mutex(void);
extern void test_suite_timer(void);
extern void test_suite_eventgroup(void);
extern void test_suite_mempool(void);
extern void test_suite_mailbox(void);
extern void test_suite_notify(void);
extern void test_suite_watchdog(void);

static uint32_t g_run    = 0;
static uint32_t g_passed = 0;
static uint32_t g_failed = 0;

static void accumulate(void) {
    g_run    += tests_run;
    g_passed += tests_passed;
    g_failed += tests_failed;
    tests_run    = 0;
    tests_passed = 0;
    tests_failed = 0;
}

int main(void)
{
    printf("\n");
    printf("######################################\n");
    printf("#   MiniRTOS Unit Test Runner        #\n");
    printf("######################################\n");

    test_suite_heap4();     accumulate();
    test_suite_task();      accumulate();
    test_suite_scheduler(); accumulate();
    test_suite_queue();     accumulate();
    test_suite_semaphore(); accumulate();
    test_suite_mutex();     accumulate();
    test_suite_timer();     accumulate();
    test_suite_eventgroup();accumulate();
    test_suite_mempool();   accumulate();
    test_suite_mailbox();   accumulate();
    test_suite_notify();    accumulate();
    test_suite_watchdog();  accumulate();

    printf("\n######################################\n");
    printf("#  TOTAL: %u | PASSED: %u | FAILED: %u\n",
           g_run, g_passed, g_failed);
    printf("######################################\n");

    return (g_failed > 0) ? 1 : 0;
}
