#ifndef OS_HEAP4_H
#define OS_HEAP4_H

#include "os_types.h"

/* ========== Heap-4 Memory Management ========== */

/*
 * Heap-4: Coalescing block allocator with first-fit strategy.
 *
 * Memory layout:
 *   [Block Header][Block Header][Block Header]...
 *   Each block has a header describing its size and allocation status.
 *   Adjacent free blocks are merged (coalesced) on free.
 *
 * Block Header Structure (simplified):
 *   - size:     Size of the usable data area (excludes header)
 *   - allocated: 1 if allocated, 0 if free
 *   - next:     Pointer to next block in the list
 */

typedef struct os_block_header {
    struct os_block_header *next;       /* Next block in the list */
    uint32_t               size;        /* Usable size (excluding header) */
    uint8_t                allocated;   /* 1 = allocated, 0 = free */
    uint8_t                reserved[3]; /* Padding for alignment */
} os_block_header_t;

/* ========== Heap API ========== */

/*
 * Initialize the heap. Must be called before any alloc/free.
 */
void os_heap_init(void);

/*
 * Allocate memory from the heap.
 * Returns NULL on failure.
 */
void* os_heap_alloc(uint32_t size);

/*
 * Allocate memory and zero it.
 * Returns NULL on failure.
 */
void* os_heap_calloc(uint32_t count, uint32_t size);

/*
 * Free previously allocated memory.
 */
void os_heap_free(void *ptr);

/*
 * Reallocate memory to a new size.
 * Returns NULL on failure (original block is preserved).
 */
void* os_heap_realloc(void *ptr, uint32_t new_size);

/*
 * Get total free heap size (approximate, due to fragmentation).
 */
uint32_t os_heap_get_free_size(void);

/*
 * Get the largest contiguous free block.
 */
uint32_t os_heap_get_largest_free_block(void);

/*
 * Get minimum free heap size ever seen (high-water mark).
 */
uint32_t os_heap_get_min_free_size(void);

#endif /* OS_HEAP4_H */
