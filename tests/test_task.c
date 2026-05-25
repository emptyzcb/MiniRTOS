/*
 * test_task.c - Unit tests for task management
 */

#include "test_framework.h"
#include "os.h"
#include "task.h"
#include "heap4.h"

/* Dummy task function */
static void dummy_task(void *param) { (void)param; while(1) {} }
static void dummy_task2(void *param) { (void)param; while(1) {} }

/* Helper: initialize kernel subsystems needed for task tests */
static void task_test_setup(void) {
    os_heap_init();
    os_task_init_ready_list();
    os_sched_init();
}

TEST_CASE(test_task_create) {
    task_test_setup();
    os_stack_t stack[128];
    os_task_handle_t handle = NULL;
    os_status_t ret = os_task_create(dummy_task, "T1", NULL, 2,
                                     stack, sizeof(stack), &handle);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_NOT_NULL(handle);
}

TEST_CASE(test_task_create_invalid_prio) {
    task_test_setup();
    os_stack_t stack[128];
    os_status_t ret = os_task_create(dummy_task, "T1", NULL,
                                     OS_CONFIG_NUM_PRIORITIES,
                                     stack, sizeof(stack), NULL);
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, ret);
}

TEST_CASE(test_task_create_null_func) {
    task_test_setup();
    os_stack_t stack[128];
    os_status_t ret = os_task_create(NULL, "T1", NULL, 2,
                                     stack, sizeof(stack), NULL);
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, ret);
}

TEST_CASE(test_task_create_stack_too_small) {
    task_test_setup();
    os_stack_t stack[16]; /* Too small */
    os_status_t ret = os_task_create(dummy_task, "T1", NULL, 2,
                                     stack, sizeof(stack), NULL);
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, ret);
}

TEST_CASE(test_task_create_max_tasks) {
    task_test_setup();
    os_stack_t stacks[OS_CONFIG_MAX_TASKS][128];
    os_status_t ret;
    /* Fill up all task slots */
    for (int i = 0; i < OS_CONFIG_MAX_TASKS; i++) {
        ret = os_task_create(dummy_task, "T", NULL, 2,
                             stacks[i], sizeof(stacks[i]), NULL);
        if (ret != OS_OK) break;
    }
    /* Next one should fail */
    os_stack_t extra[128];
    ret = os_task_create(dummy_task, "TX", NULL, 2,
                         extra, sizeof(extra), NULL);
    TEST_ASSERT_EQUAL(OS_ERR_FULL, ret);
}

TEST_CASE(test_task_delete_other) {
    task_test_setup();
    os_stack_t stack1[128], stack2[128];
    os_task_handle_t h1, h2;
    os_task_create(dummy_task, "T1", NULL, 2, stack1, sizeof(stack1), &h1);
    os_task_create(dummy_task2, "T2", NULL, 3, stack2, sizeof(stack2), &h2);

    /* Need to set a current task so critical sections work */
    os_task_set_current((os_tcb_t*)h1);

    os_status_t ret = os_task_delete(h2);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    /* After deletion, the task state should be DELETED */
    /* Note: h2 TCB memory is freed, so we can't dereference it.
     * The fact that os_task_delete returned OS_OK is sufficient. */
}

TEST_CASE(test_task_suspend_resume) {
    task_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create(dummy_task, "T1", NULL, 2, stack, sizeof(stack), &h);
    os_task_set_current((os_tcb_t*)h);

    os_task_suspend(h);
    TEST_ASSERT_EQUAL(OS_TASK_SUSPENDED, os_task_get_state(h));

    os_task_resume(h);
    TEST_ASSERT_EQUAL(OS_TASK_READY, os_task_get_state(h));
}

TEST_CASE(test_task_set_priority) {
    task_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create(dummy_task, "T1", NULL, 2, stack, sizeof(stack), &h);
    os_task_set_current((os_tcb_t*)h);

    os_task_set_priority(h, 5);
    TEST_ASSERT_EQUAL(5, os_task_get_priority(h));
}

TEST_CASE(test_task_get_state) {
    task_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create(dummy_task, "T1", NULL, 2, stack, sizeof(stack), &h);

    TEST_ASSERT_EQUAL(OS_TASK_READY, os_task_get_state(h));
}

TEST_CASE(test_task_get_count) {
    task_test_setup();
    os_stack_t stack1[128], stack2[128];
    os_task_create(dummy_task, "T1", NULL, 2, stack1, sizeof(stack1), NULL);
    TEST_ASSERT_EQUAL(1, os_task_get_count());

    os_task_create(dummy_task2, "T2", NULL, 3, stack2, sizeof(stack2), NULL);
    TEST_ASSERT_EQUAL(2, os_task_get_count());
}

TEST_CASE(test_task_get_name) {
    task_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_task_create(dummy_task, "MyTask", NULL, 2, stack, sizeof(stack), &h);

    const char *name = os_task_get_name(h);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL(0, strcmp(name, "MyTask"));
}

TEST_CASE(test_task_create_suspended) {
    task_test_setup();
    os_stack_t stack[128];
    os_task_handle_t h;
    os_status_t ret = os_task_create_suspended(dummy_task, "TS", NULL, 2,
                                               stack, sizeof(stack), &h);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(OS_TASK_SUSPENDED, os_task_get_state(h));
}

void test_suite_task(void) {
    printf("\n=== Test Suite: Task Management ===\n");
    RUN_TEST(test_task_create);
    RUN_TEST(test_task_create_invalid_prio);
    RUN_TEST(test_task_create_null_func);
    RUN_TEST(test_task_create_stack_too_small);
    RUN_TEST(test_task_create_max_tasks);
    RUN_TEST(test_task_delete_other);
    RUN_TEST(test_task_suspend_resume);
    RUN_TEST(test_task_set_priority);
    RUN_TEST(test_task_get_state);
    RUN_TEST(test_task_get_count);
    RUN_TEST(test_task_get_name);
    RUN_TEST(test_task_create_suspended);
    printf("=== Results: %u/%u passed, %u failed ===\n",
           tests_passed, tests_run, tests_failed);
}
