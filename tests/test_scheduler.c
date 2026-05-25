/*
 * test_scheduler.c - Unit tests for scheduler
 */

#include "test_framework.h"
#include "os.h"
#include "scheduler.h"
#include "task.h"
#include "heap4.h"

static void dummy_task(void *param) { (void)param; while(1) {} }
static void dummy_task2(void *param) { (void)param; while(1) {} }

static void sched_test_setup(void) {
    os_heap_init();
    os_task_init_ready_list();
    os_sched_init();
}

TEST_CASE(test_sched_critical_nesting) {
    sched_test_setup();

    TEST_ASSERT_EQUAL(0, os_sched_get_critical_nesting());

    os_sched_enter_critical();
    TEST_ASSERT_EQUAL(1, os_sched_get_critical_nesting());

    os_sched_enter_critical();
    TEST_ASSERT_EQUAL(2, os_sched_get_critical_nesting());

    os_sched_exit_critical();
    TEST_ASSERT_EQUAL(1, os_sched_get_critical_nesting());

    os_sched_exit_critical();
    TEST_ASSERT_EQUAL(0, os_sched_get_critical_nesting());
}

TEST_CASE(test_sched_select_highest) {
    sched_test_setup();
    os_stack_t stack1[128], stack2[128];
    os_task_handle_t h1, h2;
    os_task_create(dummy_task, "LOW", NULL, 5, stack1, sizeof(stack1), &h1);
    os_task_create(dummy_task2, "HIGH", NULL, 1, stack2, sizeof(stack2), &h2);

    os_sched_select_next();
    os_task_handle_t current = os_task_get_current();
    /* Higher priority (lower number) should be selected */
    TEST_ASSERT_EQUAL(h2, current);
}

TEST_CASE(test_sched_is_running) {
    sched_test_setup();
    TEST_ASSERT(!os_sched_is_running());
}

TEST_CASE(test_sched_request_switch_from_isr) {
    sched_test_setup();
    /* Should not crash */
    os_sched_request_switch_from_isr();
    TEST_ASSERT(1);
}

void test_suite_scheduler(void) {
    printf("\n=== Test Suite: Scheduler ===\n");
    RUN_TEST(test_sched_critical_nesting);
    RUN_TEST(test_sched_select_highest);
    RUN_TEST(test_sched_is_running);
    RUN_TEST(test_sched_request_switch_from_isr);
    printf("=== Results: %u/%u passed, %u failed ===\n",
           tests_passed, tests_run, tests_failed);
}
