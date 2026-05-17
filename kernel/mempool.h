/*
 * mempool.h - Fixed-Size Block Memory Pool Allocator
 *
 * O(1) alloc/free, zero fragmentation within the pool.
 * Best suited for high-frequency allocation of same-sized objects
 * (e.g. TCBs, message buffers, network packets).
 */

#ifndef OS_MEMPOOL_H
#define OS_MEMPOOL_H

#include "os_types.h"
#include "os_config.h"

#if OS_CONFIG_USE_MEMPOOL

/* ========== Memory Pool Control Block ========== */

typedef struct os_mempool {
    uint8_t     *pool_start;        /* Start address of the pool memory */
    uint8_t     *pool_end;          /* End address (one past last byte) */
    uint32_t    block_size;         /* Usable block size in bytes (user-facing) */
    uint32_t    total_blocks;       /* Total number of blocks */
    uint32_t    free_count;         /* Current number of free blocks */
    uint32_t    min_free_count;     /* Minimum free blocks ever (high-water mark) */
    void        *free_list;         /* Head of the free list (embedded in free blocks) */
} os_mempool_t;

/* ========== Memory Pool API ========== */

/*
 * Create a memory pool.
 *
 * The caller provides a pre-allocated buffer. The pool will divide it into
 * fixed-size blocks. Each block has a hidden overhead of sizeof(void*) bytes
 * for the free-list pointer, so the actual usable size of each block is:
 *     usable = block_size
 * The buffer must be large enough to hold at least one block + overhead.
 *
 * Parameters:
 *   pool        - Pointer to an os_mempool_t structure to initialize
 *   buf         - Pointer to the memory buffer for the pool
 *   buf_size    - Total size of the buffer in bytes
 *   block_size  - Desired usable block size in bytes
 *
 * Returns:
 *   OS_OK on success, OS_ERR_PARAM if parameters are invalid,
 *   OS_ERR_NOMEM if the buffer is too small.
 */
os_status_t os_mempool_create(os_mempool_t *pool, void *buf,
                              uint32_t buf_size, uint32_t block_size);

/*
 * Allocate one block from the pool.
 *
 * Returns a pointer to the block, or NULL if the pool is exhausted.
 * This operation is O(1).
 */
void* os_mempool_alloc(os_mempool_t *pool);

/*
 * Free a block back to the pool.
 *
 * The pointer must have been obtained from os_mempool_alloc() on the same pool.
 * This operation is O(1).
 */
os_status_t os_mempool_free(os_mempool_t *pool, void *ptr);

/*
 * Get the number of free blocks in the pool.
 */
uint32_t os_mempool_get_free_count(os_mempool_t *pool);

/*
 * Get the minimum number of free blocks ever seen (high-water mark).
 */
uint32_t os_mempool_get_min_free_count(os_mempool_t *pool);

/*
 * Get the total number of blocks in the pool.
 */
uint32_t os_mempool_get_total_count(os_mempool_t *pool);

/*
 * Check if a pointer belongs to this pool.
 */
bool os_mempool_owns(os_mempool_t *pool, void *ptr);

#endif /* OS_CONFIG_USE_MEMPOOL */

#endif /* OS_MEMPOOL_H */
