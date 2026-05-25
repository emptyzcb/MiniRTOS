/*
 * test_watchdog.c - Unit tests for software watchdog
 */

#include "test_framework.h"
#include "os.h"
#include "watchdog.h"
#include "task.h"
#include "heap4.h"

static volatile int wdt_timeout_count = 0;

static void wdt_test_cb(os_task_handle_t handle, void *param) {
    (void)handle;
    (void)param;
    wdt_timeout_count++;
}

static void wdt_test_setup(void) {
    os_heap_init();
    os_task_init_ready_list();
    os_sched_init();
    os_wdt_init();
    os_stack_t *idle_stack = (os_stack_t*)os_heap_alloc(256);
    os_task_handle_t idle_h;
    os_task_create((os_task_func_t)0x1, "IDLE", NULL, OS_PRIO_LOWEST,
                   idle_stack, 256, &idle_h);
    os_task_set_current((os_tcb_t*)idle_h);
    wdt_timeout_count = 0;
}

TEST_CASE(test_wdt_register) {
    wdt_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "T1", NULL, 2, stack, sizeof(stack), &h);

    os_status_t ret = os_wdt_register(h, 100, wdt_test_cb, NULL);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT(os_wdt_is_registered(h));
}

TEST_CASE(test_wdt_feed) {
    wdt_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "T1", NULL, 2, stack, sizeof(stack), &h);
    os_task_set_current((os_tcb_t*)h);

    os_wdt_register(h, 100, wdt_test_cb, NULL);

    /* Simulate some ticks passing */
    for (int i = 0; i < 50; i++) os_wdt_tick();
    TEST_ASSERT(os_wdt_get_remaining(h) < 100);

    /* Feed should reset */
    os_wdt_feed();
    TEST_ASSERT_EQUAL(100, os_wdt_get_remaining(h));
}

TEST_CASE(test_wdt_timeout) {
    wdt_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "T1", NULL, 2, stack, sizeof(stack), &h);

    os_wdt_register(h, 5, wdt_test_cb, NULL);

    /* Simulate 5 ticks - should trigger timeout */
    for (int i = 0; i < 5; i++) os_wdt_tick();
    TEST_ASSERT(wdt_timeout_count > 0);
}

TEST_CASE(test_wdt_unregister) {
    wdt_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "T1", NULL, 2, stack, sizeof(stack), &h);

    os_wdt_register(h, 100, wdt_test_cb, NULL);
    TEST_ASSERT(os_wdt_is_registered(h));

    os_status_t ret = os_wdt_unregister(h);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT(!os_wdt_is_registered(h));
}

TEST_CASE(test_wdt_pause_resume) {
    wdt_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "T1", NULL, 2, stack, sizeof(stack), &h);

    os_wdt_register(h, 10, wdt_test_cb, NULL);

    os_wdt_pause(h);
    os_tick_t before = os_wdt_get_remaining(h);

    /* Ticks should not decrement while paused */
    for (int i = 0; i < 5; i++) os_wdt_tick();
    TEST_ASSERT_EQUAL(before, os_wdt_get_remaining(h));

    os_wdt_resume(h);
    TEST_ASSERT_EQUAL(10, os_wdt_get_remaining(h));
}

TEST_CASE(test_wdt_set_deadline) {
    wdt_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "T1", NULL, 2, stack, sizeof(stack), &h);

    os_wdt_register(h, 100, wdt_test_cb, NULL);
    os_wdt_set_deadline(h, 200);
    TEST_ASSERT_EQUAL(200, os_wdt_get_remaining(h));
}

TEST_CASE(test_wdt_param_errors) {
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_wdt_register(NULL, 100, NULL, NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_wdt_register((os_task_handle_t)0x1, 0, NULL, NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_wdt_unregister(NULL));
    TEST_ASSERT(!os_wdt_is_registered(NULL));
    TEST_ASSERT_EQUAL(0, os_wdt_get_remaining(NULL));
}

void test_suite_watchdog(void) {
    printf("\n=== Test Suite: Software Watchdog ===\n");
    RUN_TEST(test_wdt_register);
    RUN_TEST(test_wdt_feed);
    RUN_TEST(test_wdt_timeout);
    RUN_TEST(test_wdt_unregister);
    RUN_TEST(test_wdt_pause_resume);
    RUN_TEST(test_wdt_set_deadline);
    RUN_TEST(test_wdt_param_errors);
    printf("=== Results: %u/%u passed, %u failed ===\n",
           tests_passed, tests_run, tests_failed);
}
