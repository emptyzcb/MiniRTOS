/*
 * test_eventgroup.c - Unit tests for event group
 */

#include "test_framework.h"
#include "os.h"
#include "eventgroup.h"
#include "task.h"
#include "heap4.h"

static void eg_test_setup(void) {
    os_heap_init();
    os_task_init_ready_list();
    os_sched_init();
    os_stack_t *idle_stack = (os_stack_t*)os_heap_alloc(256);
    os_task_handle_t idle_h;
    os_task_create((os_task_func_t)0x1, "IDLE", NULL, OS_PRIO_LOWEST,
                   idle_stack, 256, &idle_h);
    os_task_set_current((os_tcb_t*)idle_h);
}

TEST_CASE(test_eventgroup_create) {
    eg_test_setup();
    os_eventgroup_t eg;
    os_status_t ret = os_eventgroup_create(&eg);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(0, os_eventgroup_get_bits(&eg));
}

TEST_CASE(test_eventgroup_set_get) {
    eg_test_setup();
    os_eventgroup_t eg;
    os_eventgroup_create(&eg);

    os_eventgroup_set_bits(&eg, 0x01);
    TEST_ASSERT_EQUAL(0x01, os_eventgroup_get_bits(&eg));

    os_eventgroup_set_bits(&eg, 0x04);
    TEST_ASSERT_EQUAL(0x05, os_eventgroup_get_bits(&eg));
}

TEST_CASE(test_eventgroup_clear) {
    eg_test_setup();
    os_eventgroup_t eg;
    os_eventgroup_create(&eg);

    os_eventgroup_set_bits(&eg, 0x0F);
    os_eventgroup_clear_bits(&eg, 0x05);
    TEST_ASSERT_EQUAL(0x0A, os_eventgroup_get_bits(&eg));
}

TEST_CASE(test_eventgroup_clear_from_isr) {
    eg_test_setup();
    os_eventgroup_t eg;
    os_eventgroup_create(&eg);

    os_eventgroup_set_bits(&eg, 0x0F);
    os_status_t ret = os_eventgroup_clear_bits_from_isr(&eg, 0x03);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(0x0C, os_eventgroup_get_bits(&eg));
}

TEST_CASE(test_eventgroup_get_from_isr) {
    eg_test_setup();
    os_eventgroup_t eg;
    os_eventgroup_create(&eg);

    os_eventgroup_set_bits(&eg, 0xA5);
    TEST_ASSERT_EQUAL(0xA5, os_eventgroup_get_bits_from_isr(&eg));
}

TEST_CASE(test_eventgroup_wait_any_immediate) {
    eg_test_setup();
    os_eventgroup_t eg;
    os_eventgroup_create(&eg);

    os_eventgroup_set_bits(&eg, 0x03);

    uint32_t bits = os_eventgroup_wait_bits(&eg, 0x01, OS_EVENT_WAIT_ANY,
                                             OS_WAIT_NONE);
    TEST_ASSERT(bits & 0x01);
}

TEST_CASE(test_eventgroup_wait_all_immediate) {
    eg_test_setup();
    os_eventgroup_t eg;
    os_eventgroup_create(&eg);

    os_eventgroup_set_bits(&eg, 0x03);

    uint32_t bits = os_eventgroup_wait_bits(&eg, 0x03, OS_EVENT_WAIT_ALL,
                                             OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(0x03, bits & 0x03);
}

TEST_CASE(test_eventgroup_wait_none) {
    eg_test_setup();
    os_eventgroup_t eg;
    os_eventgroup_create(&eg);

    /* No bits set, WAIT_NONE timeout -> return 0 */
    uint32_t bits = os_eventgroup_wait_bits(&eg, 0x01, OS_EVENT_WAIT_ANY,
                                             OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(0, bits);
}

TEST_CASE(test_eventgroup_clear_on_exit) {
    eg_test_setup();
    os_eventgroup_t eg;
    os_eventgroup_create(&eg);

    os_eventgroup_set_bits(&eg, 0x07);

    uint32_t bits = os_eventgroup_wait_bits(&eg, 0x03,
                                             OS_EVENT_WAIT_ALL | OS_EVENT_CLEAR_ON_EXIT,
                                             OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(0x03, bits & 0x03);
    /* Bits 0x03 should have been cleared */
    TEST_ASSERT_EQUAL(0x04, os_eventgroup_get_bits(&eg));
}

TEST_CASE(test_eventgroup_delete) {
    eg_test_setup();
    os_eventgroup_t eg;
    os_eventgroup_create(&eg);
    os_eventgroup_set_bits(&eg, 0xFF);

    os_status_t ret = os_eventgroup_delete(&eg);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(0, os_eventgroup_get_bits(&eg));
}

TEST_CASE(test_eventgroup_param_errors) {
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_eventgroup_create(NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_eventgroup_delete(NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_eventgroup_set_bits(NULL, 0x01));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_eventgroup_clear_bits(NULL, 0x01));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_eventgroup_clear_bits_from_isr(NULL, 0x01));
    TEST_ASSERT_EQUAL(0, os_eventgroup_get_bits(NULL));
    TEST_ASSERT_EQUAL(0, os_eventgroup_get_bits_from_isr(NULL));
}

void test_suite_eventgroup(void) {
    printf("\n=== Test Suite: Event Group ===\n");
    RUN_TEST(test_eventgroup_create);
    RUN_TEST(test_eventgroup_set_get);
    RUN_TEST(test_eventgroup_clear);
    RUN_TEST(test_eventgroup_clear_from_isr);
    RUN_TEST(test_eventgroup_get_from_isr);
    RUN_TEST(test_eventgroup_wait_any_immediate);
    RUN_TEST(test_eventgroup_wait_all_immediate);
    RUN_TEST(test_eventgroup_wait_none);
    RUN_TEST(test_eventgroup_clear_on_exit);
    RUN_TEST(test_eventgroup_delete);
    RUN_TEST(test_eventgroup_param_errors);
    printf("=== Results: %u/%u passed, %u failed ===\n",
           tests_passed, tests_run, tests_failed);
}
