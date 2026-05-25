/*
 * test_heap4.c - Unit tests for Heap-4 memory allocator
 */

#include "test_framework.h"
#include "heap4.h"
#include "os_config.h"

TEST_CASE(test_heap_init) {
    os_heap_init();
    uint32_t free_size = os_heap_get_free_size();
    TEST_ASSERT(free_size > 0);
    TEST_ASSERT(free_size <= OS_CONFIG_HEAP_SIZE);
}

TEST_CASE(test_heap_alloc_basic) {
    os_heap_init();
    void *p = os_heap_alloc(128);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT(os_heap_get_free_size() < OS_CONFIG_HEAP_SIZE);
    os_heap_free(p);
}

TEST_CASE(test_heap_alloc_zero) {
    os_heap_init();
    void *p = os_heap_alloc(0);
    TEST_ASSERT_NULL(p);
}

TEST_CASE(test_heap_alloc_oversize) {
    os_heap_init();
    void *p = os_heap_alloc(OS_CONFIG_HEAP_SIZE + 1);
    TEST_ASSERT_NULL(p);
}

TEST_CASE(test_heap_free_basic) {
    os_heap_init();
    uint32_t before = os_heap_get_free_size();
    void *p = os_heap_alloc(256);
    TEST_ASSERT_NOT_NULL(p);
    uint32_t during = os_heap_get_free_size();
    TEST_ASSERT(during < before);
    os_heap_free(p);
    /* After free, free size should increase (block returned to pool) */
    uint32_t after = os_heap_get_free_size();
    TEST_ASSERT(after > during);
}

TEST_CASE(test_heap_free_null) {
    os_heap_init();
    /* Should not crash */
    os_heap_free(NULL);
    TEST_ASSERT(1);
}

TEST_CASE(test_heap_coalesce) {
    os_heap_init();
    void *a = os_heap_alloc(256);
    void *b = os_heap_alloc(256);
    void *c = os_heap_alloc(256);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    os_heap_free(a);
    os_heap_free(c);
    uint32_t before = os_heap_get_largest_free_block();

    os_heap_free(b);  /* a+b+c should coalesce */
    uint32_t after = os_heap_get_largest_free_block();

    TEST_ASSERT(after > before);
}

TEST_CASE(test_heap_calloc_zero) {
    os_heap_init();
    uint8_t *p = (uint8_t*)os_heap_calloc(1, 64);
    TEST_ASSERT_NOT_NULL(p);
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_EQUAL(0, p[i]);
    }
    os_heap_free(p);
}

TEST_CASE(test_heap_min_free) {
    os_heap_init();
    uint32_t initial = os_heap_get_min_free_size();
    os_heap_alloc(512);
    uint32_t after = os_heap_get_min_free_size();
    TEST_ASSERT(after <= initial);
    TEST_ASSERT(after < os_heap_get_free_size() + 512);
}

TEST_CASE(test_heap_alignment) {
    os_heap_init();
    void *p = os_heap_alloc(13);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL(0, ((uintptr_t)p) % OS_CONFIG_HEAP_ALIGNMENT);
    os_heap_free(p);
}

TEST_CASE(test_heap_realloc_expand) {
    os_heap_init();
    uint8_t *p = (uint8_t*)os_heap_alloc(64);
    TEST_ASSERT_NOT_NULL(p);
    for (int i = 0; i < 64; i++) p[i] = (uint8_t)i;

    uint8_t *p2 = (uint8_t*)os_heap_realloc(p, 128);
    TEST_ASSERT_NOT_NULL(p2);
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_EQUAL((uint8_t)i, p2[i]);
    }
    os_heap_free(p2);
}

TEST_CASE(test_heap_realloc_shrink) {
    os_heap_init();
    uint8_t *p = (uint8_t*)os_heap_alloc(128);
    TEST_ASSERT_NOT_NULL(p);
    for (int i = 0; i < 128; i++) p[i] = (uint8_t)i;

    uint8_t *p2 = (uint8_t*)os_heap_realloc(p, 64);
    TEST_ASSERT_NOT_NULL(p2);
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_EQUAL((uint8_t)i, p2[i]);
    }
    os_heap_free(p2);
}

TEST_CASE(test_heap_realloc_null) {
    os_heap_init();
    void *p = os_heap_realloc(NULL, 128);
    TEST_ASSERT_NOT_NULL(p);
    os_heap_free(p);
}

TEST_CASE(test_heap_multiple_alloc_free) {
    os_heap_init();
    uint32_t initial = os_heap_get_free_size();
    void *blocks[8];
    for (int i = 0; i < 8; i++) {
        blocks[i] = os_heap_alloc(128);
        TEST_ASSERT_NOT_NULL(blocks[i]);
    }
    uint32_t during = os_heap_get_free_size();
    for (int i = 0; i < 8; i++) {
        os_heap_free(blocks[i]);
    }
    /* All blocks freed: largest free block should be nearly the full heap */
    uint32_t largest = os_heap_get_largest_free_block();
    TEST_ASSERT(largest >= initial);
    TEST_ASSERT(os_heap_get_free_size() > during);
}

void test_suite_heap4(void) {
    printf("\n=== Test Suite: Heap-4 ===\n");
    RUN_TEST(test_heap_init);
    RUN_TEST(test_heap_alloc_basic);
    RUN_TEST(test_heap_alloc_zero);
    RUN_TEST(test_heap_alloc_oversize);
    RUN_TEST(test_heap_free_basic);
    RUN_TEST(test_heap_free_null);
    RUN_TEST(test_heap_coalesce);
    RUN_TEST(test_heap_calloc_zero);
    RUN_TEST(test_heap_min_free);
    RUN_TEST(test_heap_alignment);
    RUN_TEST(test_heap_realloc_expand);
    RUN_TEST(test_heap_realloc_shrink);
    RUN_TEST(test_heap_realloc_null);
    RUN_TEST(test_heap_multiple_alloc_free);
    printf("=== Results: %u/%u passed, %u failed ===\n",
           tests_passed, tests_run, tests_failed);
}
