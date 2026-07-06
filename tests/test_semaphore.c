/*
 * test_semaphore.c - Unit tests for semaphore
 */

#include "test_framework.h"
#include "os.h"
#include "semaphore.h"
#include "task.h"
#include "heap4.h"

static void sem_test_setup(void) {
    os_heap_init();
    os_task_init_ready_list();
    os_sched_init();
    os_stack_t *idle_stack = (os_stack_t*)os_heap_alloc(256);
    os_task_handle_t idle_h;
    os_task_create((os_task_func_t)0x1, "IDLE", NULL, OS_PRIO_LOWEST,
                   idle_stack, 256, &idle_h);
    os_task_set_current((os_tcb_t*)idle_h);
}

TEST_CASE(test_sem_create_binary) {
    sem_test_setup();
    os_sem_t sem;
    os_status_t ret = os_sem_create_binary(&sem);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(0, os_sem_get_count(&sem));
}

TEST_CASE(test_sem_create_counting) {
    sem_test_setup();
    os_sem_t sem;
    os_status_t ret = os_sem_create_counting(&sem, 10, 5);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(5, os_sem_get_count(&sem));
}

TEST_CASE(test_sem_give_take) {
    sem_test_setup();
    os_sem_t sem;
    os_sem_create_counting(&sem, 10, 0);

    os_sem_give(&sem);
    TEST_ASSERT_EQUAL(1, os_sem_get_count(&sem));

    os_sem_give(&sem);
    TEST_ASSERT_EQUAL(2, os_sem_get_count(&sem));

    os_sem_take(&sem, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(1, os_sem_get_count(&sem));

    os_sem_take(&sem, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(0, os_sem_get_count(&sem));
}

TEST_CASE(test_sem_take_from_isr) {
    sem_test_setup();
    os_sem_t sem;
    os_sem_create_counting(&sem, 3, 2);

    os_status_t ret = os_sem_take_from_isr(&sem);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(1, os_sem_get_count(&sem));

    ret = os_sem_take_from_isr(&sem);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(0, os_sem_get_count(&sem));

    ret = os_sem_take_from_isr(&sem);
    TEST_ASSERT_EQUAL(OS_ERR_EMPTY, ret);
}

TEST_CASE(test_sem_get_count_from_isr) {
    sem_test_setup();
    os_sem_t sem;
    os_sem_create_counting(&sem, 5, 3);

    TEST_ASSERT_EQUAL(3, os_sem_get_count_from_isr(&sem));
}

TEST_CASE(test_sem_take_empty) {
    sem_test_setup();
    os_sem_t sem;
    os_sem_create_binary(&sem);

    os_status_t ret = os_sem_take(&sem, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_ERR_EMPTY, ret);
}

TEST_CASE(test_sem_give_max) {
    sem_test_setup();
    os_sem_t sem;
    os_sem_create_counting(&sem, 2, 2);

    os_status_t ret = os_sem_give(&sem);
    TEST_ASSERT_EQUAL(OS_ERR_FULL, ret);
}

TEST_CASE(test_sem_binary_limit) {
    sem_test_setup();
    os_sem_t sem;
    os_sem_create_binary(&sem);

    os_sem_give(&sem);
    TEST_ASSERT_EQUAL(1, os_sem_get_count(&sem));

    os_status_t ret = os_sem_give(&sem);
    TEST_ASSERT_EQUAL(OS_ERR_FULL, ret);
}

TEST_CASE(test_sem_delete) {
    sem_test_setup();
    os_sem_t sem;
    os_sem_create_counting(&sem, 10, 3);

    os_status_t ret = os_sem_delete(&sem);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(0, os_sem_get_count(&sem));
}

TEST_CASE(test_sem_param_errors) {
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_sem_create_binary(NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_sem_create_counting(NULL, 10, 5));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_sem_take(NULL, OS_WAIT_NONE));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_sem_take_from_isr(NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_sem_give(NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_sem_delete(NULL));
    TEST_ASSERT_EQUAL(0, os_sem_get_count_from_isr(NULL));
}

void test_suite_semaphore(void) {
    printf("\n=== Test Suite: Semaphore ===\n");
    RUN_TEST(test_sem_create_binary);
    RUN_TEST(test_sem_create_counting);
    RUN_TEST(test_sem_give_take);
    RUN_TEST(test_sem_take_from_isr);
    RUN_TEST(test_sem_get_count_from_isr);
    RUN_TEST(test_sem_take_empty);
    RUN_TEST(test_sem_give_max);
    RUN_TEST(test_sem_binary_limit);
    RUN_TEST(test_sem_delete);
    RUN_TEST(test_sem_param_errors);
    printf("=== Results: %u/%u passed, %u failed ===\n",
           tests_passed, tests_run, tests_failed);
}
