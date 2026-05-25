/*
 * test_timer.c - Unit tests for software timer
 *
 * NOTE: Timer callbacks run in the timer service task, which is not
 * actually scheduled during host-side unit tests. Tests verify timer
 * state management (create/start/stop/active) without relying on
 * callback execution or os_timer_tick().
 */

#include "test_framework.h"
#include "os.h"
#include "timer.h"
#include "task.h"
#include "heap4.h"

static void test_timer_cb(os_timer_t *timer) {
    (void)timer;
}

static void timer_test_setup(void) {
    os_heap_init();
    os_task_init_ready_list();
    os_sched_init();
    os_stack_t *idle_stack = (os_stack_t*)os_heap_alloc(256);
    os_task_handle_t idle_h;
    os_task_create((os_task_func_t)0x1, "IDLE", NULL, OS_PRIO_LOWEST,
                   idle_stack, 256, &idle_h);
    os_task_set_current((os_tcb_t*)idle_h);
}

TEST_CASE(test_timer_create) {
    timer_test_setup();
    os_timer_t tmr;
    os_status_t ret = os_timer_create(&tmr, "T1", 100, OS_TIMER_ONE_SHOT,
                                       test_timer_cb);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT(!os_timer_is_active(&tmr));
}

TEST_CASE(test_timer_start_stop) {
    timer_test_setup();
    os_timer_t tmr;
    os_timer_create(&tmr, "T1", 100, OS_TIMER_ONE_SHOT, test_timer_cb);

    os_timer_start(&tmr, OS_WAIT_NONE);
    TEST_ASSERT(os_timer_is_active(&tmr));

    os_timer_stop(&tmr, OS_WAIT_NONE);
    TEST_ASSERT(!os_timer_is_active(&tmr));
}

TEST_CASE(test_timer_start_already_running) {
    timer_test_setup();
    os_timer_t tmr;
    os_timer_create(&tmr, "T1", 100, OS_TIMER_ONE_SHOT, test_timer_cb);

    os_timer_start(&tmr, OS_WAIT_NONE);
    TEST_ASSERT(os_timer_is_active(&tmr));

    /* Starting again should be idempotent */
    os_timer_start(&tmr, OS_WAIT_NONE);
    TEST_ASSERT(os_timer_is_active(&tmr));
}

TEST_CASE(test_timer_stop_not_running) {
    timer_test_setup();
    os_timer_t tmr;
    os_timer_create(&tmr, "T1", 100, OS_TIMER_ONE_SHOT, test_timer_cb);

    /* Stopping a non-running timer should be safe */
    os_timer_stop(&tmr, OS_WAIT_NONE);
    TEST_ASSERT(!os_timer_is_active(&tmr));
}

TEST_CASE(test_timer_reset) {
    timer_test_setup();
    os_timer_t tmr;
    os_timer_create(&tmr, "T1", 100, OS_TIMER_ONE_SHOT, test_timer_cb);
    os_timer_start(&tmr, OS_WAIT_NONE);

    /* Reset should keep it active */
    os_timer_reset(&tmr, OS_WAIT_NONE);
    TEST_ASSERT(os_timer_is_active(&tmr));
}

TEST_CASE(test_timer_reset_not_running) {
    timer_test_setup();
    os_timer_t tmr;
    os_timer_create(&tmr, "T1", 100, OS_TIMER_ONE_SHOT, test_timer_cb);

    /* Reset on inactive timer should be a no-op */
    os_timer_reset(&tmr, OS_WAIT_NONE);
    TEST_ASSERT(!os_timer_is_active(&tmr));
}

TEST_CASE(test_timer_change_period) {
    timer_test_setup();
    os_timer_t tmr;
    os_timer_create(&tmr, "T1", 100, OS_TIMER_ONE_SHOT, test_timer_cb);
    os_timer_start(&tmr, OS_WAIT_NONE);

    os_timer_change_period(&tmr, 200, OS_WAIT_NONE);
    TEST_ASSERT(os_timer_is_active(&tmr));
}

TEST_CASE(test_timer_is_active) {
    timer_test_setup();
    os_timer_t tmr;
    os_timer_create(&tmr, "T1", 100, OS_TIMER_ONE_SHOT, test_timer_cb);

    TEST_ASSERT(!os_timer_is_active(&tmr));
    os_timer_start(&tmr, OS_WAIT_NONE);
    TEST_ASSERT(os_timer_is_active(&tmr));
    os_timer_stop(&tmr, OS_WAIT_NONE);
    TEST_ASSERT(!os_timer_is_active(&tmr));
}

TEST_CASE(test_timer_delete) {
    timer_test_setup();
    os_timer_t tmr;
    os_timer_create(&tmr, "T1", 100, OS_TIMER_ONE_SHOT, test_timer_cb);
    os_timer_start(&tmr, OS_WAIT_NONE);

    os_timer_delete(&tmr, OS_WAIT_NONE);
    TEST_ASSERT(!os_timer_is_active(&tmr));
}

TEST_CASE(test_timer_param_errors) {
    os_timer_t tmr;
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_timer_create(NULL, "T1", 10,
                                                      OS_TIMER_ONE_SHOT,
                                                      test_timer_cb));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_timer_create(&tmr, "T1", 10,
                                                      OS_TIMER_ONE_SHOT, NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_timer_create(&tmr, "T1", 0,
                                                      OS_TIMER_ONE_SHOT,
                                                      test_timer_cb));
    TEST_ASSERT(!os_timer_is_active(NULL));
}

void test_suite_timer(void) {
    printf("\n=== Test Suite: Software Timer ===\n");
    RUN_TEST(test_timer_create);
    RUN_TEST(test_timer_start_stop);
    RUN_TEST(test_timer_start_already_running);
    RUN_TEST(test_timer_stop_not_running);
    RUN_TEST(test_timer_reset);
    RUN_TEST(test_timer_reset_not_running);
    RUN_TEST(test_timer_change_period);
    RUN_TEST(test_timer_is_active);
    RUN_TEST(test_timer_delete);
    RUN_TEST(test_timer_param_errors);
    printf("=== Results: %u/%u passed, %u failed ===\n",
           tests_passed, tests_run, tests_failed);
}
