/*
 * test_notify.c - Unit tests for task notification
 */

#include "test_framework.h"
#include "os.h"
#include "notify.h"
#include "task.h"
#include "heap4.h"

static void notify_test_setup(void) {
    os_heap_init();
    os_task_init_ready_list();
    os_sched_init();
    os_stack_t *idle_stack = (os_stack_t*)os_heap_alloc(256);
    os_task_handle_t idle_h;
    os_task_create((os_task_func_t)0x1, "IDLE", NULL, OS_PRIO_LOWEST,
                   idle_stack, 256, &idle_h);
    os_task_set_current((os_tcb_t*)idle_h);
}

TEST_CASE(test_notify_send) {
    notify_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "T1", NULL, 2, stack, sizeof(stack), &h);

    os_status_t ret = os_task_notify(h, 42);
    TEST_ASSERT_EQUAL(OS_OK, ret);

    /* Notification should be pending */
    os_tcb_t *tcb = (os_tcb_t*)h;
    TEST_ASSERT_EQUAL(1, tcb->notify_pending);
    TEST_ASSERT_EQUAL(42, tcb->notify_value);
    TEST_ASSERT(os_task_notify_is_pending(h));
}

TEST_CASE(test_notify_overwrite) {
    notify_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "T1", NULL, 2, stack, sizeof(stack), &h);

    os_task_notify(h, 10);
    os_task_notify(h, 20);

    os_tcb_t *tcb = (os_tcb_t*)h;
    TEST_ASSERT_EQUAL(20, tcb->notify_value);
}

TEST_CASE(test_notify_clear) {
    notify_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "T1", NULL, 2, stack, sizeof(stack), &h);

    os_task_notify(h, 77);
    TEST_ASSERT(os_task_notify_is_pending(h));

    os_status_t ret = os_task_notify_clear(h);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT(!os_task_notify_is_pending(h));

    os_tcb_t *tcb = (os_tcb_t*)h;
    TEST_ASSERT_EQUAL(0, tcb->notify_pending);
    TEST_ASSERT_EQUAL(0, tcb->notify_value);
}

TEST_CASE(test_notify_wait_immediate) {
    notify_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "T1", NULL, 2, stack, sizeof(stack), &h);
    os_task_set_current((os_tcb_t*)h);

    /* Send notification first */
    os_task_notify(h, 99);

    /* Wait should return immediately */
    uint32_t val = 0;
    os_status_t ret = os_task_notify_wait(&val, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(99, val);
    TEST_ASSERT(!os_task_notify_is_pending(h));
}

TEST_CASE(test_notify_wait_timeout) {
    notify_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "T1", NULL, 2, stack, sizeof(stack), &h);
    os_task_set_current((os_tcb_t*)h);

    /* No notification pending, WAIT_NONE -> timeout */
    uint32_t val = 0;
    os_status_t ret = os_task_notify_wait(&val, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_ERR_TIMEOUT, ret);
}

TEST_CASE(test_notify_param_errors) {
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_task_notify(NULL, 0));
    TEST_ASSERT(!os_task_notify_is_pending(NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_task_notify_clear(NULL));
}

void test_suite_notify(void) {
    printf("\n=== Test Suite: Task Notification ===\n");
    RUN_TEST(test_notify_send);
    RUN_TEST(test_notify_overwrite);
    RUN_TEST(test_notify_clear);
    RUN_TEST(test_notify_wait_immediate);
    RUN_TEST(test_notify_wait_timeout);
    RUN_TEST(test_notify_param_errors);
    printf("=== Results: %u/%u passed, %u failed ===\n",
           tests_passed, tests_run, tests_failed);
}
