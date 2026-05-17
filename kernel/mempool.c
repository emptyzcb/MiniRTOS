/*
 * mempool.c - Fixed-Size Block Memory Pool Allocator
 *
 * A memory pool divides a contiguous buffer into fixed-size blocks.
 * Free blocks are linked together in a singly-linked free list,
 * with the next-pointer stored directly inside the free block's data area.
 *
 * Properties:
 *   - O(1) allocation: pop head of free list
 *   - O(1) free: push to head of free list
 *   - Zero fragmentation (all blocks are the same size)
 *   - Minimal overhead: sizeof(void*) per block (only when free)
 *
 * Memory layout of the pool:
 *
 *   +----------+----------+----------+-----+----------+
 *   | Block 0  | Block 1  | Block 2  | ... | Block N  |
 *   +----------+----------+----------+-----+----------+
 *
 * Each block layout:
 *   - Allocated: [user data (block_size bytes)]
 *   - Free:      [next pointer | padding]  (first sizeof(void*) bytes used for free list)
 */

#include "mempool.h"
#include "os_config.h"
#include <string.h>

#if OS_CONFIG_USE_MEMPOOL

/* Minimum block size must hold at least a pointer (for the free list) */
#define MIN_BLOCK_SIZE  sizeof(void*)

/* Align size up to pointer boundary */
#define POOL_ALIGN_SIZE(s)  (((s) + sizeof(void*) - 1) & ~(sizeof(void*) - 1))

/* ========== Internal Helpers ========== */

/*
 * Get the actual stride per block: user block_size rounded up to pointer alignment,
 * plus no extra header (the free-list pointer lives inside the block when free).
 * But we need the stride to be at least sizeof(void*) so blocks don't overlap.
 */
static uint32_t prv_calc_stride(uint32_t block_size)
{
    uint32_t stride = POOL_ALIGN_SIZE(block_size);
    if (stride < MIN_BLOCK_SIZE) {
        stride = MIN_BLOCK_SIZE;
    }
    return stride;
}

/* ========== Public API ========== */

os_status_t os_mempool_create(os_mempool_t *pool, void *buf,
                              uint32_t buf_size, uint32_t block_size)
{
    uint8_t *aligned_buf;
    uint32_t stride;
    uint32_t num_blocks;
    uint32_t i;

    if (pool == NULL || buf == NULL || buf_size == 0 || block_size == 0) {
        return OS_ERR_PARAM;
    }

    /* Align the buffer start to pointer boundary */
    aligned_buf = (uint8_t*)POOL_ALIGN_SIZE((uint32_t)buf);
    if (aligned_buf < (uint8_t*)buf) {
        aligned_buf += sizeof(void*);
    }

    /* Adjust available size after alignment */
    uint32_t usable = buf_size - (uint32_t)(aligned_buf - (uint8_t*)buf);
    if (usable < MIN_BLOCK_SIZE) {
        return OS_ERR_NOMEM;
    }

    stride = prv_calc_stride(block_size);
    num_blocks = usable / stride;

    if (num_blocks == 0) {
        return OS_ERR_NOMEM;
    }

    /* Initialize the pool metadata */
    pool->pool_start    = aligned_buf;
    pool->pool_end      = aligned_buf + num_blocks * stride;
    pool->block_size    = block_size;
    pool->total_blocks  = num_blocks;
    pool->free_count    = num_blocks;
    pool->min_free_count = num_blocks;

    /* Build the free list: each free block's first sizeof(void*) bytes
     * points to the next free block. The last one points to NULL. */
    pool->free_list = NULL;

    for (i = 0; i < num_blocks; i++) {
        void *block = aligned_buf + i * stride;
        *(void**)block = pool->free_list;
        pool->free_list = block;
    }

    return OS_OK;
}

void* os_mempool_alloc(os_mempool_t *pool)
{
    void *block;

    if (pool == NULL || pool->free_count == 0) {
        return NULL;
    }

    /* Pop head of free list */
    block = pool->free_list;
    pool->free_list = *(void**)block;
    pool->free_count--;

    /* Update high-water mark */
    if (pool->free_count < pool->min_free_count) {
        pool->min_free_count = pool->free_count;
    }

    return block;
}

os_status_t os_mempool_free(os_mempool_t *pool, void *ptr)
{
    if (pool == NULL || ptr == NULL) {
        return OS_ERR_PARAM;
    }

    /* Bounds check: pointer must belong to this pool */
    if ((uint8_t*)ptr < pool->pool_start || (uint8_t*)ptr >= pool->pool_end) {
        return OS_ERR_PARAM;
    }

    /* Alignment check: pointer must be at a valid block boundary */
    uint32_t offset = (uint32_t)((uint8_t*)ptr - pool->pool_start);
    uint32_t stride = prv_calc_stride(pool->block_size);
    if ((offset % stride) != 0) {
        return OS_ERR_PARAM;
    }

    /* Push to head of free list */
    *(void**)ptr = pool->free_list;
    pool->free_list = ptr;
    pool->free_count++;

    return OS_OK;
}

uint32_t os_mempool_get_free_count(os_mempool_t *pool)
{
    if (pool == NULL) {
        return 0;
    }
    return pool->free_count;
}

uint32_t os_mempool_get_min_free_count(os_mempool_t *pool)
{
    if (pool == NULL) {
        return 0;
    }
    return pool->min_free_count;
}

uint32_t os_mempool_get_total_count(os_mempool_t *pool)
{
    if (pool == NULL) {
        return 0;
    }
    return pool->total_blocks;
}

bool os_mempool_owns(os_mempool_t *pool, void *ptr)
{
    if (pool == NULL || ptr == NULL) {
        return false;
    }
    return ((uint8_t*)ptr >= pool->pool_start && (uint8_t*)ptr < pool->pool_end);
}

#endif /* OS_CONFIG_USE_MEMPOOL */
