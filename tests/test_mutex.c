/*
 * test_mutex.c - Unit tests for mutex
 */

#include "test_framework.h"
#include "os.h"
#include "mutex.h"
#include "task.h"
#include "heap4.h"

static void mutex_test_setup(void) {
    os_heap_init();
    os_task_init_ready_list();
    os_sched_init();
    os_stack_t *idle_stack = (os_stack_t*)os_heap_alloc(256);
    os_task_handle_t idle_h;
    os_task_create((os_task_func_t)0x1, "IDLE", NULL, OS_PRIO_LOWEST,
                   idle_stack, 256, &idle_h);
    os_task_set_current((os_tcb_t*)idle_h);
}

TEST_CASE(test_mutex_create) {
    mutex_test_setup();
    os_mutex_t mtx;
    os_status_t ret = os_mutex_create(&mtx);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_NULL(os_mutex_get_owner(&mtx));
    TEST_ASSERT(!os_mutex_is_locked(&mtx));
    TEST_ASSERT_EQUAL(0, os_mutex_get_lock_count(&mtx));
}

TEST_CASE(test_mutex_lock_unlock) {
    mutex_test_setup();
    os_mutex_t mtx;
    os_mutex_create(&mtx);

    os_status_t ret = os_mutex_lock(&mtx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_NOT_NULL(os_mutex_get_owner(&mtx));
    TEST_ASSERT(os_mutex_is_locked(&mtx));
    TEST_ASSERT_EQUAL(1, os_mutex_get_lock_count(&mtx));

    ret = os_mutex_unlock(&mtx);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_NULL(os_mutex_get_owner(&mtx));
    TEST_ASSERT(!os_mutex_is_locked(&mtx));
    TEST_ASSERT_EQUAL(0, os_mutex_get_lock_count(&mtx));
}

TEST_CASE(test_mutex_recursive) {
    mutex_test_setup();
    os_mutex_t mtx;
    os_mutex_create(&mtx);

    os_mutex_lock(&mtx, OS_WAIT_NONE);
    os_mutex_lock(&mtx, OS_WAIT_NONE);
    os_mutex_lock(&mtx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(3, os_mutex_get_lock_count(&mtx));

    /* Should unlock 3 times before fully released */
    os_mutex_unlock(&mtx);
    TEST_ASSERT_NOT_NULL(os_mutex_get_owner(&mtx));
    TEST_ASSERT_EQUAL(2, os_mutex_get_lock_count(&mtx));

    os_mutex_unlock(&mtx);
    TEST_ASSERT_NOT_NULL(os_mutex_get_owner(&mtx));
    TEST_ASSERT_EQUAL(1, os_mutex_get_lock_count(&mtx));

    os_mutex_unlock(&mtx);
    TEST_ASSERT_NULL(os_mutex_get_owner(&mtx));
    TEST_ASSERT_EQUAL(0, os_mutex_get_lock_count(&mtx));
}

TEST_CASE(test_mutex_unlock_not_owner) {
    mutex_test_setup();
    os_mutex_t mtx;
    os_mutex_create(&mtx);

    /* Lock with current task */
    os_mutex_lock(&mtx, OS_WAIT_NONE);

    /* Create another task and set it as current */
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "OTHER", NULL, 2, stack, sizeof(stack), &h);
    os_task_set_current((os_tcb_t*)h);

    os_status_t ret = os_mutex_unlock(&mtx);
    TEST_ASSERT_EQUAL(OS_ERR_STATE, ret);

    /* Restore original owner to clean up */
    os_task_set_current((os_tcb_t*)os_mutex_get_owner(&mtx));
    os_mutex_unlock(&mtx);
}

TEST_CASE(test_mutex_would_block) {
    mutex_test_setup();
    os_mutex_t mtx;
    os_mutex_create(&mtx);

    /* Lock with current task */
    os_mutex_lock(&mtx, OS_WAIT_NONE);

    /* Try to lock from a "different" task with no wait */
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create((os_task_func_t)0x2, "OTHER", NULL, 2, stack, sizeof(stack), &h);
    os_task_set_current((os_tcb_t*)h);

    os_status_t ret = os_mutex_lock(&mtx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_ERR_WOULD_BLOCK, ret);

    /* Clean up */
    os_task_set_current((os_tcb_t*)os_mutex_get_owner(&mtx));
    os_mutex_unlock(&mtx);
}

TEST_CASE(test_mutex_delete) {
    mutex_test_setup();
    os_mutex_t mtx;
    os_mutex_create(&mtx);
    os_mutex_lock(&mtx, OS_WAIT_NONE);

    os_status_t ret = os_mutex_delete(&mtx);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_NULL(os_mutex_get_owner(&mtx));
}

TEST_CASE(test_mutex_param_errors) {
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mutex_create(NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mutex_lock(NULL, OS_WAIT_NONE));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mutex_unlock(NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mutex_delete(NULL));
    TEST_ASSERT_NULL(os_mutex_get_owner(NULL));
    TEST_ASSERT(!os_mutex_is_locked(NULL));
    TEST_ASSERT_EQUAL(0, os_mutex_get_lock_count(NULL));
}

void test_suite_mutex(void) {
    printf("\n=== Test Suite: Mutex ===\n");
    RUN_TEST(test_mutex_create);
    RUN_TEST(test_mutex_lock_unlock);
    RUN_TEST(test_mutex_recursive);
    RUN_TEST(test_mutex_unlock_not_owner);
    RUN_TEST(test_mutex_would_block);
    RUN_TEST(test_mutex_delete);
    RUN_TEST(test_mutex_param_errors);
    printf("=== Results: %u/%u passed, %u failed ===\n",
           tests_passed, tests_run, tests_failed);
}
