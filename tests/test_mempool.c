/*
 * test_mempool.c - Unit tests for memory pool
 */

#include "test_framework.h"
#include "os.h"
#include "mempool.h"
#include "heap4.h"

static void mempool_test_setup(void) {
    os_heap_init();
}

TEST_CASE(test_mempool_create) {
    mempool_test_setup();
    os_mempool_t pool;
    uint8_t buf[512];
    os_status_t ret = os_mempool_create(&pool, buf, sizeof(buf), 32);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT(os_mempool_get_total_count(&pool) > 0);
    TEST_ASSERT_EQUAL(os_mempool_get_total_count(&pool),
                       os_mempool_get_free_count(&pool));
}

TEST_CASE(test_mempool_alloc_free) {
    mempool_test_setup();
    os_mempool_t pool;
    uint8_t buf[512];
    os_mempool_create(&pool, buf, sizeof(buf), 32);

    uint32_t free_before = os_mempool_get_free_count(&pool);

    void *block = os_mempool_alloc(&pool);
    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_EQUAL(free_before - 1, os_mempool_get_free_count(&pool));

    os_status_t ret = os_mempool_free(&pool, block);
    TEST_ASSERT_EQUAL(OS_OK, ret);
    TEST_ASSERT_EQUAL(free_before, os_mempool_get_free_count(&pool));
}

TEST_CASE(test_mempool_exhaust) {
    mempool_test_setup();
    os_mempool_t pool;
    uint8_t buf[256];
    os_mempool_create(&pool, buf, sizeof(buf), 32);

    uint32_t total = os_mempool_get_total_count(&pool);
    void *blocks[32];
    uint32_t allocated = 0;

    for (uint32_t i = 0; i < total && i < 32; i++) {
        blocks[i] = os_mempool_alloc(&pool);
        if (blocks[i] != NULL) allocated++;
    }

    TEST_ASSERT_EQUAL(0, os_mempool_get_free_count(&pool));

    /* Next alloc should fail */
    void *p = os_mempool_alloc(&pool);
    TEST_ASSERT_NULL(p);

    /* Free all */
    for (uint32_t i = 0; i < allocated; i++) {
        os_mempool_free(&pool, blocks[i]);
    }
    TEST_ASSERT_EQUAL(total, os_mempool_get_free_count(&pool));
}

TEST_CASE(test_mempool_owns) {
    mempool_test_setup();
    os_mempool_t pool;
    uint8_t buf[512];
    os_mempool_create(&pool, buf, sizeof(buf), 32);

    void *block = os_mempool_alloc(&pool);
    TEST_ASSERT(os_mempool_owns(&pool, block));

    uint8_t outside = 0;
    TEST_ASSERT(!os_mempool_owns(&pool, &outside));
    TEST_ASSERT(!os_mempool_owns(&pool, NULL));

    os_mempool_free(&pool, block);
}

TEST_CASE(test_mempool_min_free) {
    mempool_test_setup();
    os_mempool_t pool;
    uint8_t buf[512];
    os_mempool_create(&pool, buf, sizeof(buf), 32);

    uint32_t total = os_mempool_get_total_count(&pool);
    TEST_ASSERT_EQUAL(total, os_mempool_get_min_free_count(&pool));

    void *block = os_mempool_alloc(&pool);
    TEST_ASSERT_EQUAL(total - 1, os_mempool_get_min_free_count(&pool));

    os_mempool_free(&pool, block);
    /* min_free should not increase */
    TEST_ASSERT_EQUAL(total - 1, os_mempool_get_min_free_count(&pool));
}

TEST_CASE(test_mempool_alignment) {
    mempool_test_setup();
    os_mempool_t pool;
    uint8_t buf[512];
    os_mempool_create(&pool, buf, sizeof(buf), 13);

    void *block = os_mempool_alloc(&pool);
    TEST_ASSERT_NOT_NULL(block);
    /* Block should be pointer-aligned */
    TEST_ASSERT_EQUAL(0, ((uintptr_t)block) % sizeof(void*));
    os_mempool_free(&pool, block);
}

TEST_CASE(test_mempool_param_errors) {
    os_mempool_t pool;
    uint8_t buf[64];
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mempool_create(NULL, buf, 64, 16));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mempool_create(&pool, NULL, 64, 16));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mempool_create(&pool, buf, 0, 16));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mempool_create(&pool, buf, 64, 0));
    TEST_ASSERT_NULL(os_mempool_alloc(NULL));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mempool_free(NULL, buf));
    TEST_ASSERT_EQUAL(OS_ERR_PARAM, os_mempool_free(&pool, NULL));
    TEST_ASSERT_EQUAL(0, os_mempool_get_free_count(NULL));
    TEST_ASSERT_EQUAL(0, os_mempool_get_total_count(NULL));
    TEST_ASSERT(!os_mempool_owns(NULL, buf));
}

void test_suite_mempool(void) {
    printf("\n=== Test Suite: Memory Pool ===\n");
    RUN_TEST(test_mempool_create);
    RUN_TEST(test_mempool_alloc_free);
    RUN_TEST(test_mempool_exhaust);
    RUN_TEST(test_mempool_owns);
    RUN_TEST(test_mempool_min_free);
    RUN_TEST(test_mempool_alignment);
    RUN_TEST(test_mempool_param_errors);
    printf("=== Results: %u/%u passed, %u failed ===\n",
           tests_passed, tests_run, tests_failed);
}
