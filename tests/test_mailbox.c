/*
 * test_mailbox.c - Unit tests for mailbox
 */

#include "test_framework.h"
#include "os.h"
#include "mailbox.h"
#include "task.h"
#include "heap4.h"

static void mb_test_setup(void) {
    os_heap_init();
    os_task_init_ready_list();
    os_sched_init();
    os_stack_t *idle_stack = (os_stack_t*)os_heap_alloc(256);
    os_task_handle_t idle_h;
    os_task_create((os_task_func_t)0x1, "IDLE", NULL, OS_PRIO_LOWEST,
                   idle_stack, 256, &idle_h);
    os_task_set_current((os_tcb_t*)idle_h);
}

TEST_CASE(test_mailbox_create) {
    mb_test_setup();
    os_mailbox_t mb;
    os_status_t ret = os_mailbox_create(&mb, sizeof(uint32_t));
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT(os_mailbox_is_empty(&mb));
    TEST_ASSERT(!os_mailbox_is_full(&mb));
    os_mailbox_delete(&mb);
}

TEST_CASE(test_mailbox_send_receive) {
    mb_test_setup();
    os_mailbox_t mb;
    os_mailbox_create(&mb, sizeof(uint32_t));

    uint32_t tx = 0xABCD;
    os_status_t ret = os_mailbox_send(&mb, &tx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT(os_mailbox_is_full(&mb));

    uint32_t rx = 0;
    ret = os_mailbox_receive(&mb, &rx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(0xABCD, rx);
    TEST_ASSERT(os_mailbox_is_empty(&mb));

    os_mailbox_delete(&mb);
}

TEST_CASE(test_mailbox_full) {
    mb_test_setup();
    os_mailbox_t mb;
    os_mailbox_create(&mb, sizeof(uint32_t));

    uint32_t v = 1;
    os_mailbox_send(&mb, &v, OS_WAIT_NONE);

    /* Second send should fail with WAIT_NONE */
    v = 2;
    os_status_t ret = os_mailbox_send(&mb, &v, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_ERR_FULL, ret);

    os_mailbox_delete(&mb);
}

TEST_CASE(test_mailbox_empty) {
    mb_test_setup();
    os_mailbox_t mb;
    os_mailbox_create(&mb, sizeof(uint32_t));

    uint32_t rx;
    os_status_t ret = os_mailbox_receive(&mb, &rx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_ERR_EMPTY, ret);

    os_mailbox_delete(&mb);
}

TEST_CASE(test_mailbox_overwrite) {
    mb_test_setup();
    os_mailbox_t mb;
    os_mailbox_create(&mb, sizeof(uint32_t));

    uint32_t v = 100;
    os_mailbox_send(&mb, &v, OS_WAIT_NONE);

    /* Overwrite is not supported in this implementation (blocks on full).
     * But we can test that the single-element semantics work correctly. */
    uint32_t rx;
    os_mailbox_receive(&mb, &rx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(100, rx);

    /* Now send a new value */
    v = 200;
    os_mailbox_send(&mb, &v, OS_WAIT_NONE);
    os_mailbox_receive(&mb, &rx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(200, rx);

    os_mailbox_delete(&mb);
}

TEST_CASE(test_mailbox_param_errors) {
    os_mailbox_t mb;
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mailbox_create(NULL, 4));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mailbox_create(&mb, 0));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mailbox_delete(NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mailbox_send(NULL, &(int){1}, 0));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mailbox_send(&mb, NULL, 0));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mailbox_receive(NULL, &(int){1}, 0));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mailbox_receive(&mb, NULL, 0));
    TEST_ASSERT(os_mailbox_is_empty(NULL));
    TEST_ASSERT(!os_mailbox_is_full(NULL));
}

void test_suite_mailbox(void) {
    printf("\n=== Test Suite: Mailbox ===\n");
    RUN_TEST(test_mailbox_create);
    RUN_TEST(test_mailbox_send_receive);
    RUN_TEST(test_mailbox_full);
    RUN_TEST(test_mailbox_empty);
    RUN_TEST(test_mailbox_overwrite);
    RUN_TEST(test_mailbox_param_errors);
    printf("=== Results: %u/%u passed, %u failed ===\n",
           tests_passed, tests_run, tests_failed);
}
