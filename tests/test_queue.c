/*
 * test_queue.c - Unit tests for message queue
 */

#include "test_framework.h"
#include "os.h"
#include "queue.h"
#include "task.h"
#include "heap4.h"

static void queue_test_setup(void) {
    os_heap_init();
    os_task_init_ready_list();
    os_sched_init();
    /* Set up a dummy current task so critical sections work */
    os_stack_t *idle_stack = (os_stack_t*)os_heap_alloc(256);
    os_task_handle_t idle_h;
    os_task_create((os_task_func_t)0x1, "IDLE", NULL, OS_PRIO_LOWEST,
                   idle_stack, 256, &idle_h);
    os_task_set_current((os_tcb_t*)idle_h);
}

TEST_CASE(test_queue_create) {
    queue_test_setup();
    os_queue_t q;
    os_status_t ret = os_queue_create(&q, sizeof(uint32_t), 10);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(0, os_queue_get_count(&q));
    TEST_ASSERT_EQUAL(10, os_queue_get_spaces(&q));
    os_queue_delete(&q);
}

TEST_CASE(test_queue_send_receive) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, sizeof(uint32_t), 10);

    uint32_t tx = 42;
    os_status_t ret = os_queue_send(&q, &tx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(1, os_queue_get_count(&q));

    uint32_t rx = 0;
    ret = os_queue_receive(&q, &rx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(42, rx);
    TEST_ASSERT_EQUAL(0, os_queue_get_count(&q));

    os_queue_delete(&q);
}

TEST_CASE(test_queue_full) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, sizeof(uint32_t), 2);

    uint32_t v = 1;
    os_queue_send(&q, &v, OS_WAIT_NONE);
    v = 2;
    os_queue_send(&q, &v, OS_WAIT_NONE);

    v = 3;
    os_status_t ret = os_queue_send(&q, &v, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_ERR_FULL, ret);

    os_queue_delete(&q);
}

TEST_CASE(test_queue_empty) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, sizeof(uint32_t), 10);

    uint32_t rx;
    os_status_t ret = os_queue_receive(&q, &rx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_ERR_EMPTY, ret);

    os_queue_delete(&q);
}

TEST_CASE(test_queue_fifo_order) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, sizeof(uint32_t), 10);

    for (uint32_t i = 1; i <= 5; i++) {
        os_queue_send(&q, &i, OS_WAIT_NONE);
    }

    for (uint32_t i = 1; i <= 5; i++) {
        uint32_t rx;
        os_queue_receive(&q, &rx, OS_WAIT_NONE);
        TEST_ASSERT_EQUAL(i, rx);
    }

    os_queue_delete(&q);
}

TEST_CASE(test_queue_peek) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, sizeof(uint32_t), 10);

    uint32_t tx = 99;
    os_queue_send(&q, &tx, OS_WAIT_NONE);

    uint32_t rx = 0;
    os_status_t ret = os_queue_peek(&q, &rx);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(99, rx);
    /* Data should still be in queue */
    TEST_ASSERT_EQUAL(1, os_queue_get_count(&q));

    os_queue_delete(&q);
}

TEST_CASE(test_queue_count_spaces) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, sizeof(uint32_t), 5);

    TEST_ASSERT_EQUAL(0, os_queue_get_count(&q));
    TEST_ASSERT_EQUAL(5, os_queue_get_spaces(&q));

    uint32_t v = 1;
    os_queue_send(&q, &v, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(1, os_queue_get_count(&q));
    TEST_ASSERT_EQUAL(4, os_queue_get_spaces(&q));

    os_queue_delete(&q);
}

TEST_CASE(test_queue_reset) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, sizeof(uint32_t), 3);

    uint32_t v = 1;
    os_queue_send(&q, &v, OS_WAIT_NONE);
    v = 2;
    os_queue_send(&q, &v, OS_WAIT_NONE);

    os_status_t ret = os_queue_reset(&q);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(0, os_queue_get_count(&q));
    TEST_ASSERT_EQUAL(3, os_queue_get_spaces(&q));
    TEST_ASSERT_TRUE(os_queue_is_empty(&q));

    uint32_t rx = 0;
    ret = os_queue_receive(&q, &rx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_ERR_EMPTY, ret);

    os_queue_delete(&q);
}

TEST_CASE(test_queue_overwrite) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, sizeof(uint32_t), 1);

    uint32_t v = 1;
    os_queue_send(&q, &v, OS_WAIT_NONE);
    v = 2;
    os_status_t ret = os_queue_overwrite(&q, &v);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(1, os_queue_get_count(&q));

    uint32_t rx = 0;
    ret = os_queue_receive(&q, &rx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(2, rx);

    v = 3;
    ret = os_queue_overwrite(&q, &v);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    ret = os_queue_receive(&q, &rx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(3, rx);

    os_queue_delete(&q);
}

TEST_CASE(test_queue_overwrite_rejects_multi_item_queue) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, sizeof(uint32_t), 2);

    uint32_t v = 1;
    os_status_t ret = os_queue_overwrite(&q, &v);
    TEST_ASSERT_EQUAL(OS_ERR_STATE, ret);

    os_queue_delete(&q);
}

TEST_CASE(test_queue_get_count_from_isr) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, sizeof(uint32_t), 3);

    uint32_t v = 1;
    os_queue_send(&q, &v, OS_WAIT_NONE);
    v = 2;
    os_queue_send(&q, &v, OS_WAIT_NONE);

    TEST_ASSERT_EQUAL(2, os_queue_get_count_from_isr(&q));

    os_queue_delete(&q);
}

TEST_CASE(test_queue_wraparound) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, sizeof(uint32_t), 3);

    /* Fill and drain multiple times to test ring buffer wraparound */
    for (int round = 0; round < 5; round++) {
        for (uint32_t i = 1; i <= 3; i++) {
            os_queue_send(&q, &i, OS_WAIT_NONE);
        }
        for (uint32_t i = 1; i <= 3; i++) {
            uint32_t rx;
            os_queue_receive(&q, &rx, OS_WAIT_NONE);
            TEST_ASSERT_EQUAL(i, rx);
        }
    }

    os_queue_delete(&q);
}

TEST_CASE(test_queue_delete) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, sizeof(uint32_t), 10);

    uint32_t v = 1;
    os_queue_send(&q, &v, OS_WAIT_NONE);

    os_status_t ret = os_queue_delete(&q);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_NULL(q.buffer);
}

TEST_CASE(test_queue_item_size_4) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, 4, 5);

    uint32_t tx = 0xDEADBEEF;
    os_queue_send(&q, &tx, OS_WAIT_NONE);

    uint32_t rx = 0;
    os_queue_receive(&q, &rx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(0xDEADBEEF, rx);

    os_queue_delete(&q);
}

TEST_CASE(test_queue_item_size_64) {
    queue_test_setup();
    os_queue_t q;
    os_queue_create(&q, 64, 5);

    uint8_t tx[64];
    memset(tx, 0xAB, 64);
    os_queue_send(&q, tx, OS_WAIT_NONE);

    uint8_t rx[64];
    memset(rx, 0, 64);
    os_queue_receive(&q, rx, OS_WAIT_NONE);
    TEST_ASSERT_EQUAL(0, memcmp(tx, rx, 64));

    os_queue_delete(&q);
}

TEST_CASE(test_queue_param_errors) {
    queue_test_setup();
    os_queue_t q;

    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_queue_create(NULL, 4, 10));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_queue_create(&q, 0, 10));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_queue_create(&q, 4, 0));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_queue_send(NULL, &(int){1}, OS_WAIT_NONE));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_queue_receive(NULL, &(int){1}, OS_WAIT_NONE));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_queue_reset(NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_queue_overwrite(NULL, &(int){1}));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_queue_overwrite(&q, NULL));
    TEST_ASSERT_EQUAL(0, os_queue_get_count_from_isr(NULL));
}

void test_suite_queue(void) {
    printf("\n=== Test Suite: Message Queue ===\n");
    RUN_TEST(test_queue_create);
    RUN_TEST(test_queue_send_receive);
    RUN_TEST(test_queue_full);
    RUN_TEST(test_queue_empty);
    RUN_TEST(test_queue_fifo_order);
    RUN_TEST(test_queue_peek);
    RUN_TEST(test_queue_count_spaces);
    RUN_TEST(test_queue_reset);
    RUN_TEST(test_queue_overwrite);
    RUN_TEST(test_queue_overwrite_rejects_multi_item_queue);
    RUN_TEST(test_queue_get_count_from_isr);
    RUN_TEST(test_queue_wraparound);
    RUN_TEST(test_queue_delete);
    RUN_TEST(test_queue_item_size_4);
    RUN_TEST(test_queue_item_size_64);
    RUN_TEST(test_queue_param_errors);
    printf("=== Results: %u/%u passed, %u failed ===\n",
           tests_passed, tests_run, tests_failed);
}
